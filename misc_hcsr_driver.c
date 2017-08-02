#include <linux/miscdevice.h>
#include <linux/fs.h>
#include <linux/kernel.h>
#include <linux/module.h>  // Module Defines and Macros (THIS_MODULE)
#include <linux/cdev.h>    // Character Device Types and functions.
#include <linux/types.h>
#include <linux/slab.h>    // Kmalloc/Kfree
#include <asm/uaccess.h>   // Copy to/from user space
#include <linux/string.h>
#include <linux/device.h>  // Device Creation / Destruction functions
#include <linux/jiffies.h> // Timing Library
#include <linux/errno.h>
#include <linux/gpio.h>
#include <asm/gpio.h>
#include <linux/of_device.h>
#include <linux/timer.h>
#include <linux/time.h>
#include <linux/delay.h>
#include <linux/kthread.h>
#include <linux/interrupt.h>
#include <asm/msr.h>
#include <linux/semaphore.h>
#include <linux/wait.h>
#include <linux/mutex.h>
#include "hcsr04_defs.h"

//static DECLARE_SEMAPHORE_GENERIC(name, count)

#define MIN_FREQ 1
#define MAX_FREQ 16

#define RISING 1
#define FALLING 2

#define BUFF_SIZE 6


//Per-device struct for the misc device
struct hcsr04_dev
{
    struct miscdevice miscdevice; //Misc device structure
    unsigned int measurements[6]; //Per-device FIFO to store the last 5 measurements
    /*
    Note the buffer size is set to 6 since because of the nature of implementation
    of ring buffer, the head pointer must always point at an empty slot, hence one
    slot is always empty. So to store maximum 5 values at a time the buffer size has been increased
    */
    unsigned int head_ptr; //Head pointer for the ring buffer
    unsigned int tail_ptr; //Tail pointer for the ring buffer
    int trigger_pin; //The trigger pin used by the device
    int echo_pin; //The echo pin used by the device
    unsigned int echo_irq; //the IRQ number for the echo interrupt
    int mode; //The mode this device is set to 
    int sampling_frequency; //The sampling frequency (if the mode is periodic)
    int value_ready; //Flag to tell when a new value is ready from the interrupt
    int measurement_ready; //Flag to signal the wait queue
    wait_queue_head_t my_queue_1; //Wait queue for oneshot mode
    wait_queue_head_t my_queue_2; //Wait queue for periodic mode
    struct mutex buff_mutex; 
    struct timespec start; //Timespec struct to store the start time of the echo pulse
    struct timespec end; //Timespec to store the end time of the echo pulse
    struct task_struct *TriggerThread; //Task struct for the kthread (for periodic mode)
    struct hcsr04_dev *mynext; //Pointer to the next misc device
}*hcsr04_devp[2];

struct hcsr04_dev *mylist_head = NULL; //Define the misc device list head

//Trigger kthread function declaration
static int trigger_thread(void *data);

//Look up table to obtain GPIO number from board IO number
static const int pins_lookup[20] = {
                                        11, 12, 13, 14,
                                         6,  0,  1, 38,
                                        40,  4, 10,  5,
                                        15,  7, 48, 50,
                                        52, 54, 56, 58
                                   };

//Function to push a value into the ring buffer
unsigned int push(struct hcsr04_dev *r_buff, int data)
{
    int temp;
    int next;

    //Acquire the lock so no one else can make any changes to the buffer while we are pushing a new value
    mutex_lock(&(r_buff->buff_mutex));
    next = r_buff->head_ptr + 1;

    //If the buffer is full, roll over
    if (next >= BUFF_SIZE)
        next = 0;

    //If there is existing data at that position; overwrite it
    if (next == r_buff->tail_ptr)
    {
        temp = r_buff->tail_ptr + 1;
        if (temp >= BUFF_SIZE)
            temp = 0;

        r_buff->tail_ptr = temp;
        r_buff->measurements[next] = 0;
    }

    //Push the new value into the buffer
    r_buff->measurements[r_buff->head_ptr] = data;

    //printk("\n%d : Pushed: %d", next, r_buff->measurements[r_buff->head_ptr]);
    r_buff->head_ptr = next;

    //Release the mutex lock
    mutex_unlock(&(r_buff->buff_mutex));

    return 0;
}

//Function to pop a value from the top of the ring buffer
unsigned int pop(struct hcsr04_dev *r_buff)
{
    int data, next;

    //Acquire the lock so no one else can make any changes to the buffer while we are popping a new value
    mutex_lock(&(r_buff->buff_mutex));

    //If the buffer is empty...
    if (r_buff->head_ptr == r_buff->tail_ptr)
    {
        //Release the mutex lock and return
        //printk("\nNo new data in ring buffer!");
        mutex_unlock(&(r_buff->buff_mutex));
        return -1;
    }

    //Read the value at the top of the ring buffer
    data = r_buff->measurements[r_buff->tail_ptr];
    //Clear the buffer at that index
    r_buff->measurements[r_buff->tail_ptr] = 0;

    next = r_buff->tail_ptr + 1;

    //Roll over
    if (next >= BUFF_SIZE)
        next = 0;

    r_buff->tail_ptr = next;

    //Release the mutex lock
    mutex_unlock(&(r_buff->buff_mutex));

    //printk("\nPopped: %d", data);

    return data;
}

//Function to check if the ring_buffer is empty
unsigned int is_empty(struct hcsr04_dev *r_buff)
{
    int i;

    //Mutex lock to make sure that the contents of the buffer do not change while we are working with it...
    mutex_lock(&(r_buff->buff_mutex));

    //Iterate through all the values in the buffer
    for(i = 0; i < BUFF_SIZE; i++)
    {
        //If any value is non zero
        if (r_buff->measurements[i] != 0)
        {
            //Release the mutex lock and return 0
            mutex_unlock(&(r_buff->buff_mutex));
            return 0;
        }
    }
    //If the buffer is empty then release the mutex lock and return -1
    mutex_unlock(&(r_buff->buff_mutex));
    return -1;
}

//Function to dump the contents of the ring buffer (for debugging)
void dump(struct hcsr04_dev *r_buff)
{
    int i;
    printk("\n----------");
    for(i = 0; i < BUFF_SIZE; i++)
        printk("\n%d : Data = %d", i, r_buff->measurements[i]);
    printk("\n----------");
    //return 0;
}

//Function to apply a 10 us trigger pulse to the ultrasonic sensor
void apply_trigger(struct hcsr04_dev *curr)
{
    gpio_set_value_cansleep(pins_lookup[curr->trigger_pin], 0);
    udelay(2);
    gpio_set_value_cansleep(pins_lookup[curr->trigger_pin], 1);
    udelay(10);
    gpio_set_value_cansleep(pins_lookup[curr->trigger_pin], 0);
}

//Function to obtain a one-shot value from the ultra sonic sensor
void measure_oneshot(struct hcsr04_dev *curr)
{
    struct timespec diff; //Struct to store the difference between the end and start times
    unsigned long long int distance_reading; //Struct to store the calculated distance in cm

    //Reset the value ready flag
    curr->value_ready = 0;

    //Apply the trigger pulse
    apply_trigger(curr);

    //Sleep and wait for the interrupt handler to detect both rising and falling edges and record the start and end times
    wait_event_interruptible(curr->my_queue_1, curr->value_ready == 1);

    //Reset the value ready flag
    curr->value_ready = 0;

    //Calculate the difference between the end and start times
    diff = timespec_sub(curr->end, curr->start);
    
    distance_reading = diff.tv_nsec;

    //Convert time to microseconds from nano seconds
    do_div(distance_reading, 1000);

    //Calculate the distance
    do_div(distance_reading, 58);

    // printk("\n%s: Period: %d, Time (ns): %lu, Distance (cm): %llu",
    //        curr->miscdevice.name,
    //        period,
    //        diff.tv_nsec,
    //        distance_reading);
    
    //Push the distance reading to the per-device FIFO buffer
    push(curr, distance_reading);

    return;
}


static irq_handler_t echo_handler(int irq, void *dev_id, struct pt_regs *regs)
{
    struct hcsr04_dev *curr = (struct hcsr04_dev*) dev_id;

    if (curr->value_ready == 0)
    {
        if(gpio_get_value(pins_lookup[curr->echo_pin]))
        {
            getnstimeofday(&(curr->start));
            irq_set_irq_type (irq, IRQF_TRIGGER_FALLING); //set interrupt to occur on falling edge

        }
        else if (!(gpio_get_value(pins_lookup[curr->echo_pin])))
        {
            getnstimeofday(&(curr->end));
            irq_set_irq_type (irq, IRQF_TRIGGER_RISING); //set interrupt to occur on falling edge
            curr->value_ready = 1;
            wake_up_interruptible(&(curr->my_queue_1));
        }
    }

    return (irq_handler_t)IRQ_HANDLED;
}

//Function for the kthread used in periodic mode
static int trigger_thread(void *data)
{
    struct hcsr04_dev *curr = (struct hcsr04_dev*) data;

    unsigned int period;

    struct timespec diff; //Struct to store the difference between the end and start times
    unsigned long long int distance_reading; //Struct to store the calculated distance in cm

    //Calculate the sampling period based on the sampling frequency set by the user
    period = 1000/curr->sampling_frequency;

    //Repeat until the kthread is stopped
    while(!kthread_should_stop())
    {
        //If a new value is ready
        if (curr->value_ready == 1)
        {

             //Calculate the difference between the end and start times
            diff = timespec_sub(curr->end, curr->start);

            distance_reading = diff.tv_nsec;

            //Convert time to microseconds from nano seconds
            do_div(distance_reading, 1000);

            //Calculate the distance
            do_div(distance_reading, 58);

            // printk("\n%s: Period: %d, Time (ns): %lu, Distance (cm): %llu",
            //        curr->miscdevice.name,
            //        period,
            //        diff.tv_nsec,
            //        distance_reading);

           
            push(curr, distance_reading);

            if (curr->measurement_ready == 0)
            {
                curr->measurement_ready = 1;
                wake_up_interruptible(&(curr->my_queue_2));
            }

            //Reset the value ready flag
            curr->value_ready = 0;

            //Apply another trigger pulse
            apply_trigger(curr);

        }
        //Sleep for the specified amount of time
        msleep(period);
    }

    return 0;
}

//The open function
static int HCSRO4_open(struct inode *inode, struct file *file)
{
    struct hcsr04_dev *curr;
    int minor = iminor(inode); //Obtain the minor number of this device

    curr = mylist_head; //
    //printk("\nMy actual minor: %d", minor);

    //Go through the linked list and obtain the pointer to the per-device struct that matches the minor number of this device
    while(curr != NULL)
    {
        if (curr->miscdevice.minor == minor)
        {
            //printk("\n 1 My minor: %d, My name: %s.", curr->miscdevice.minor, curr->miscdevice.name);

            file->private_data = curr;
            return 0;
        }

        curr = curr->mynext;
    }

    return -1;
}

//The close function
static int HCSRO4_close(struct inode *inodep, struct file *filp)
{
    //pr_info("Sleepy time\n");
    return 0;
}

//The write function
static ssize_t HCSRO4_write(struct file *file, const char __user *buf,
                            size_t len, loff_t *ppos)
{
    struct hcsr04_dev *curr = file->private_data;
    int *num = kmalloc(sizeof(int), GFP_KERNEL); //Allocate memory for the data being copied from the user

    //Copy data from user
    if (copy_from_user(num, buf, len) != 0)
    {
       // printk("\nError: Copy from user space unsuccessful.");
        return 1;
    }
    //printk("\n %s: From user: %d", curr->miscdevice.name, *num);

    switch(curr->mode)
    {
    //If the current mode is one-shot...
    case ONESHOT:
        if (*num != 0) //If the parameter is non-zero then clear the per-device buffer
        {
            //Acquire the lock to the per-device ring buffer
            mutex_lock(&(curr->buff_mutex));
            //Clear the whole ring buffer
            memset(curr->measurements, 0, sizeof(curr->measurements));
            //Release the lock to the per-device ring buffer
            mutex_unlock(&(curr->buff_mutex));
        }

        //Trigger new measurements request if there is no on-going measuerment
        if (curr->value_ready == 0)
            measure_oneshot(curr);

        return 0;

        break;

    //If the current mode is periodic...
    case PERIODIC:
        if (*num != 0) //If the parameter is non-zero then clear the per-device buffer
        {
            //Start the kthread for periodic sampling
            curr->TriggerThread = kthread_run(trigger_thread, (void *)(curr), "work_test");
            //curr->value_ready = 0; DON'T UNCOMMENT

            //Apply the trigger
            apply_trigger(curr);
            return 0;
        }
        else
        {
            //Stop periodic sampling if the parameter is 0
            if(curr->TriggerThread != NULL)
            {
                kthread_stop(curr->TriggerThread);
                curr->TriggerThread = NULL;
            }
            return 0;
        }

    default:
        //printk("\nError: No mode set yet!");
        return -1;
    }
}


//The read function
static ssize_t HCSRO4_read(struct file *file, char *buf,
                           size_t len, loff_t *ppos)
{
    struct hcsr04_dev *curr = file->private_data;
    int ret = 0, distance_measurement;

    //If the mode is periodic...
    if (curr->mode == PERIODIC)
    {
        //If the per-device FIFO buffer is empty...
        ret = is_empty(curr);
        if (ret < 0)
        {
            //Wait for a new value
            wait_event_interruptible(curr->my_queue_2, curr->measurement_ready == 1);
            //Reset the flag
            curr->measurement_ready = 0;
            //Pop the distance measruement value from the FIFO buffer
            distance_measurement = pop(curr);
        }
        else
        {
            //Reset the flag
            curr->measurement_ready = 0;
            //Pop the value from the FIFO buffer
            distance_measurement = pop(curr);
        }
    }
  
    //Else if the mode is one-shot
    else if (curr->mode == ONESHOT)
    {
        //If the per-device FIFO buffer is empty...
        ret = is_empty(curr);
        if (ret < 0)
        {
            //Trigger new one-shot measurement
            measure_oneshot(curr);

            //Pop the distance measruement value from the FIFO buffer
            distance_measurement = pop(curr);
        }
        else
        {
            //Pop the distance measruement value from the FIFO buffer
            distance_measurement = pop(curr);
        }
    }
      
    //Copy the distance measurement data value to user  
    ret = copy_to_user(buf, &distance_measurement, sizeof(int));
    if (ret < 0)
    {

        //printk ("\nError: Unable to copy to user");
        return -1;
    }

    return 0;
}

static long HCSRO4_ioctl(struct file *file, unsigned int request, unsigned long argp)
{

    struct hcsr04_dev *curr = file->private_data;
    int ret;
    unsigned int pins[2];

    //Copy the parameters from the userspace
    if (copy_from_user(pins, (unsigned int *)argp, sizeof(pins)))
    {
        //printk("\nError: Copy from user space unsuccessful.");
        return 1;
    }

    //Switch case depending upon whether it is a SETPINS ioctl command or SETMODE ioctl command
    switch(request)
    {
    case SETPINS:
        //printk("\n %s: Echo: %d, Trigger: %d", curr->miscdevice.name, pins[0], pins[1]);

        //As an initial check make sure that the pins are not identical to each other
        if (pins[0] == pins[1])
        {
            //printk("\n %s: 1 Invalid pins!", curr->miscdevice.name);
            return -(EINVAL);
        }

        //Only allow IO1 and IO2 to be used as echo pins
        if (pins[0] != 2 && pins[0] != 1)
        {
            //printk("\n %s: 2 Invalid pins!", curr->miscdevice.name);
            return -(EINVAL);
        }

        //Only allow IO7 and IO8 to be used as trigger pins
        if (pins[1] != 7 && pins[1] != 8)
        {
            //printk("\n %s: 3 Invalid pins!", curr->miscdevice.name);
            return -(EINVAL);
        }

        //Check if they are already in use by this device
        if ((curr->echo_pin == pins[0]) && (curr->trigger_pin == pins[1]))
        {
            //tell which pins are being used
            //printk("\n Pins unchanged!");
            return 0;
        }

        //Check if the requested echo pin is already in use by another device
        ret = gpio_request(pins_lookup[pins[0]], "ECHO_PIN");
        if(ret < 0)
        {
            //printk("\n %s: Error requesting ECHO pin!", curr->miscdevice.name);
            return -(EINVAL);
        }

        //Check if the requested trigger pin is already in use by another device
        ret = gpio_request(pins_lookup[pins[1]], "TRIGGER_PIN");
        if(ret < 0)
        {
            //printk("\n %s: Error requesting TRIGGER pin!", curr->miscdevice.name);
            gpio_free(pins_lookup[pins[0]]);
            return -(EINVAL);
        }

        //Check if the pins for this device have already been configured once
        //If yes..release them
        if ((curr->echo_pin != -1) && (curr->trigger_pin != -1))
        {
            gpio_free(pins_lookup[curr->echo_pin]);
            gpio_free(pins_lookup[curr->trigger_pin]);
        }

        //Update the echo and trigger pin records in the struct for this device
        curr->echo_pin = pins[0];
        curr->trigger_pin = pins[1];

        //Set up the pins

        //For ECHO pin

        //Setup the interrupt for the echo signal.
        curr->echo_irq = gpio_to_irq(pins_lookup[curr->echo_pin]);

        //Request IRQ for the given echo pin
        ret = request_irq(curr->echo_irq, (irq_handler_t)echo_handler, IRQF_TRIGGER_RISING, "echo_Dev", (void *)(curr));
        if(ret < 0)
        {
            //printk("Error requesting IRQ: %d\n", ret);

            //If there is an error free the requested gpios and return an error
            gpio_free(pins_lookup[curr->trigger_pin]);
            gpio_free(pins_lookup[curr->echo_pin]);
            return -(EINVAL);
        }

        //For TRIGGER pin

        //Set the output direction of the given trigger pin
        ret = gpio_direction_output(pins_lookup[curr->trigger_pin], 0); //Trigger dir
        if(ret < 0)
        {
            //printk("\n %s: Error setting TRIGGER_PI as output!", curr->miscdevice.name);

            //If there is an error free the requested gpios and return an error
            gpio_free(pins_lookup[curr->echo_pin]);
            gpio_free(pins_lookup[curr->trigger_pin]);
            return -(EINVAL);
        }

        //Set the initial value of the given trigger pin to 0
        gpio_set_value_cansleep(pins_lookup[curr->trigger_pin], 0); // Set trigger to 0

        //printk("\n %s: SETPINS successful!!", curr->miscdevice.name);

        break;




    case SETMODE:
        //printk("\n %s: Mode: %d, Sampling Frequency: %d", curr->miscdevice.name, pins[0], pins[1]);

        //Make sure that the mode is either 0 (One-shot) or 1 (Periodic)
        if (pins[0] != 0 && pins[0] != 1)
        {
            //printk("\n %s: Invalid mode!", curr->miscdevice.name);
            return -1;
        }

        //If the mode is periodic...
        if (pins[0] == PERIODIC)
        {
            //Make sure that the given frequency paramater lies within a valid range
            if (pins[1] < MIN_FREQ || pins[1] > MAX_FREQ)
            {
                //printk("\n %s: Sampling frequency out of range!", curr->miscdevice.name);
                return -1;
            }
        }

        //If a kthread is already running, stop it
        if(curr->TriggerThread != NULL)
        {
            kthread_stop(curr->TriggerThread);
        }

        //Update the mode record in the per-device struct
        curr->mode = pins[0];

        //If the mode is periodic, also update the frequency record in the per-device struct
        if (curr->mode == PERIODIC)
            curr->sampling_frequency = pins[1];

        curr->measurement_ready = 0;
        //printk("\nSet mode successful");
        break;

    default:
        return -1;
    }

    return 0;
}

static const struct file_operations HCSRO4_fops =
{
    .owner      = THIS_MODULE,
    .open     = HCSRO4_open,
    .release    = HCSRO4_close,
    .write      = HCSRO4_write,
    .read       = HCSRO4_read,
    .llseek     = no_llseek,
    .unlocked_ioctl = HCSRO4_ioctl,
};

//Structs containing the dynamically assigned minor number along with the name and fops for each of the misc devices.

struct miscdevice hcsr1_device =
{
    .minor = MISC_DYNAMIC_MINOR,
    .name = "HCSR_1",
    .fops = &HCSRO4_fops,
};

struct miscdevice hcsr2_device =
{
    .minor = MISC_DYNAMIC_MINOR,
    .name = "HCSR_2",
    .fops = &HCSRO4_fops,
};


static int __init misc_init(void)
{
    int error = 0;
    int i = 0;
    
    //Allocate memory for the per-device structure for each device
    hcsr04_devp[0] = kmalloc(sizeof(struct hcsr04_dev), GFP_KERNEL);
    hcsr04_devp[1] = kmalloc(sizeof(struct hcsr04_dev), GFP_KERNEL);

    //Intialize all the per-device struct elements for the first device
    hcsr04_devp[0]->miscdevice = hcsr1_device;
    memset(hcsr04_devp[0]->measurements, 0, sizeof(hcsr04_devp[0]->measurements));
    hcsr04_devp[0]->head_ptr = 0;
    hcsr04_devp[0]->tail_ptr = 0;
    hcsr04_devp[0]->trigger_pin = -1;
    hcsr04_devp[0]->echo_pin = -1;
    hcsr04_devp[0]->echo_irq = -1;
    hcsr04_devp[0]->mode = -1;
    hcsr04_devp[0]->sampling_frequency = -1;
    hcsr04_devp[0]->value_ready = 0;
    hcsr04_devp[0]->measurement_ready = 1;
    init_waitqueue_head(&(hcsr04_devp[0]->my_queue_1));
    init_waitqueue_head(&(hcsr04_devp[0]->my_queue_2));
    mutex_init(&(hcsr04_devp[0]->buff_mutex)); 
    hcsr04_devp[0]->TriggerThread = NULL;

    hcsr04_devp[0]->mynext = hcsr04_devp[1];

    //Intialize all the per-device struct elements for the second device
    hcsr04_devp[1]->miscdevice = hcsr2_device;
    memset(hcsr04_devp[1]->measurements, 0, sizeof(hcsr04_devp[1]->measurements));
    hcsr04_devp[1]->head_ptr = 0;
    hcsr04_devp[1]->tail_ptr = 0;
    hcsr04_devp[1]->trigger_pin = -1;
    hcsr04_devp[1]->echo_pin = -1;
    hcsr04_devp[1]->echo_irq = -1;
    hcsr04_devp[1]->mode = -1;
    hcsr04_devp[1]->sampling_frequency = -1;
    hcsr04_devp[1]->value_ready = 0;
    hcsr04_devp[1]->measurement_ready = 0;
    init_waitqueue_head(&(hcsr04_devp[1]->my_queue_1));
    init_waitqueue_head(&(hcsr04_devp[1]->my_queue_2));
    mutex_init(&(hcsr04_devp[1]->buff_mutex)); 
    hcsr04_devp[1]->TriggerThread = NULL;
    hcsr04_devp[1]->mynext = NULL;

    //Update the value of the list head
    mylist_head = hcsr04_devp[0];

    //Register all misc devices...
    for(i = 0; i < N; i++)
    {
        //Register the misc device
        error = misc_register(&(hcsr04_devp[i]->miscdevice));
        if (error < 0)
        {
            pr_err("can't misc_register %s\n", hcsr04_devp[i]->miscdevice.name);
            return error;
        }
    }

    return 0;
}

static void __exit misc_exit(void)
{
    //Free the IRQ.
    if (hcsr04_devp[0]->echo_irq != -1)
        free_irq(hcsr04_devp[0]->echo_irq, (void *)(hcsr04_devp[0]));

    //Free the IRQ.
    if (hcsr04_devp[1]->echo_irq != -1)
        free_irq(hcsr04_devp[1]->echo_irq, (void *)(hcsr04_devp[1]));


    //Free trigger pins
    if (hcsr04_devp[0]->trigger_pin != -1)
        gpio_free(pins_lookup[hcsr04_devp[0]->trigger_pin]);

    if (hcsr04_devp[1]->trigger_pin != -1)
        gpio_free(pins_lookup[hcsr04_devp[1]->trigger_pin]);

    //Free echo pins
    if (hcsr04_devp[0]->echo_pin != -1)
        gpio_free(pins_lookup[hcsr04_devp[0]->echo_pin]);

    if (hcsr04_devp[1]->echo_pin != -1)
        gpio_free(pins_lookup[hcsr04_devp[1]->echo_pin]);


    //Stop kthreads
    if(hcsr04_devp[0]->TriggerThread != NULL)
    {
        kthread_stop(hcsr04_devp[0]->TriggerThread);
    }

    if(hcsr04_devp[1]->TriggerThread != NULL)
    {
        kthread_stop(hcsr04_devp[1]->TriggerThread);
    }

    //De-register misc devices
    misc_deregister(&(hcsr04_devp[0]->miscdevice));
    misc_deregister(&(hcsr04_devp[1]->miscdevice));

    //Free allocated memory
    kfree(hcsr04_devp[0]);
    kfree(hcsr04_devp[1]);
}

module_init(misc_init)
module_exit(misc_exit)

MODULE_DESCRIPTION("Simple Misc Driver");
MODULE_AUTHOR("Nick Glynn <n.s.glynn@gmail.com>");
MODULE_LICENSE("GPL");