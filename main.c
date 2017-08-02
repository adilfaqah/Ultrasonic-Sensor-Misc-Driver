#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <pthread.h>
#include <unistd.h>
#include <sys/ioctl.h>
#include <fcntl.h>
#include <time.h>
#include <errno.h>

#include "hcsr04_defs.h"

#define DEBUG 1

//Write function defines

//One shot
#define CLEAR_BUFF 1
#define DO_NOTHING 0

//Periodic
#define START_PERIODIC 1
#define STOP_PERIODIC 0

//Function for unexporting the pins
int cleanup_pins_1(void)
{
    int FdUnexport;

    //Open the unexport file
    FdUnexport = open("/sys/class/gpio/unexport", O_WRONLY);


    //Unexport all the exported pins one by one

    if (FdUnexport < 0)
    {
        printf("\n GPIO Export open failed.");
        return -1;
    }

    if(write(FdUnexport,"34", 2) < 0)
    {
        printf("\n Error: GPIO 34 Unexport.");
        return -1;   
    }

    if(write(FdUnexport,"35", 2) < 0)
    {
        printf("\n Error: GPIO 35 Unexport.");
        return -1;   
    }

    if(write(FdUnexport,"77", 2) < 0)
    {
        printf("\n Error: GPIO 77 Unexport.");
        return -1;   
    }

    close(FdUnexport);

    return 0;
}

int cleanup_pins_2(void)
{
    int FdUnexport;

    //Open the unexport file
    FdUnexport = open("/sys/class/gpio/unexport", O_WRONLY);


    //Unexport all the exported pins one by one

    if (FdUnexport < 0)
    {
        printf("\n GPIO Export open failed.");
        return -1;
    }

    if(write(FdUnexport,"28", 2) < 0)
    {
        printf("\n Error: GPIO 28 Unexport.");
        return -1;   
    }

    if(write(FdUnexport,"29", 2) < 0)
    {
        printf("\n Error: GPIO 29 Unexport.");
        return -1;   
    }

    if(write(FdUnexport,"45", 2) < 0)
    {
        printf("\n Error: GPIO 45 Unexport.");
        return -1;   
    }

    close(FdUnexport);

    return 0;
}


int setup_pins_1(void)
{
    int FdExport, fd34, fd35, fd77;

    //Open the export file
    FdExport = open("/sys/class/gpio/export", O_WRONLY);


    if (FdExport < 0)
    {
        printf("\n GPIO Export open failed.");
        return -1;
    }

    //Export GPIOs 34, 35 and 77
    if(write(FdExport,"34", 2) < 0)
    {
        printf("\n Error: GPIO 34 Export.");
        return -1;   
    }

    if(write(FdExport,"35", 2) < 0)
    {
        printf("\n Error: GPIO 35 Export.");
        return -1;   
    }

    if(write(FdExport,"77", 2) < 0)
    {
        printf("\n Error: GPIO 77 Export.");
        return -1;   
    }

    close(FdExport);

    //Open the direction file for GPIOs 34 and 35
    fd34 = open("/sys/class/gpio/gpio34/direction", O_WRONLY);
    if (fd34 < 0)
    {
        printf("\n Error: GPIO 34 direction open.");
        return -1;
    }

    fd35 = open("/sys/class/gpio/gpio35/direction", O_WRONLY);
    if (fd35 < 0)
    {
        printf("\n Error: GPIO 35 direction open.");
        return -1;
    }

    //Set GPIOs 34 and 35 as outputs
    if(write(fd34,"out", 3) < 0)
    {
        printf("\n Error: GPIO 34 direction out.");
        return -1;   
    }

    if(write(fd35,"out", 3) < 0)
    {
        printf("\n Error: GPIO 35 direction out.");
        return -1;   
    }

    close(fd34);
    close(fd35);


    //Open the value file for GPIOs 34, 35 and 77.
    fd34 = open("/sys/class/gpio/gpio34/value", O_WRONLY);
    if (fd34 < 0)
    {
        printf("\n Error: GPIO 34 value open.");
        return -1;
    }

    fd35 = open("/sys/class/gpio/gpio35/value", O_WRONLY);
    if (fd35 < 0)
    {
        printf("\n Error: GPIO 35 value open.");
        return -1;
    }

    fd77 = open("/sys/class/gpio/gpio77/value", O_WRONLY);
    if (fd77 < 0)
    {
        printf("\n Error: GPIO 77 value open.");
        return -1;
    }
  
    //Write 1 to GPIO 34. Write 0 to GPIO 35 and 77.
    if(write(fd34,"1", 1) < 0)
    {
        printf("\n Error: GPIO 34 value.");
        return -1;   
    }

    if(write(fd35,"0", 1) < 0)
    {
        printf("\n Error: GPIO 35 value.");
        return -1;   
    }

    if(write(fd77,"0", 1) < 0)
    {
        printf("\n Error: GPIO 77 value.");
        return -1;   
    }

    close(fd34);
    close(fd35);
    close(fd77);

    return 0;
}

int setup_pins_2(void)
{
    int FdExport, fd28, fd29, fd45;

    //Open the export file
    FdExport = open("/sys/class/gpio/export", O_WRONLY);

    if (FdExport < 0)
    {
        printf("\n GPIO Export open failed.");
        return -1;
    }

    //Export GPIOs 28, 29 and 45
    if(write(FdExport,"28", 2) < 0)
    {
        printf("\n Error: GPIO 28 Export.");
        return -1;   
    }

    if(write(FdExport,"29", 2) < 0)
    {
        printf("\n Error: GPIO 29 Export.");
        return -1;   
    }

    if(write(FdExport,"45", 2) < 0)
    {
        printf("\n Error: GPIO 45 Export.");
        return -1;   
    }

    close(FdExport);

    //Open the direction file for GPIOs 28 and 29
    fd28 = open("/sys/class/gpio/gpio28/direction", O_WRONLY);
    if (fd28 < 0)
    {
        printf("\n Error: GPIO 28 direction open.");
        return -1;
    }

    fd29 = open("/sys/class/gpio/gpio29/direction", O_WRONLY);
    if (fd29 < 0)
    {
        printf("\n Error: GPIO 29 direction open.");
        return -1;
    }

    //Set GPIOs 28 and 29 as output
    if(write(fd28,"out", 3) < 0)
    {
        printf("\n Error: GPIO 28 direction out.");
        return -1;   
    }

    if(write(fd29,"out", 3) < 0)
    {
        printf("\n Error: GPIO 29 direction out.");
        return -1;   
    }

    close(fd28);
    close(fd29);


    //Open the value file for GPIO 28, 29 and 45
    fd28 = open("/sys/class/gpio/gpio28/value", O_WRONLY);
    if (fd28 < 0)
    {
        printf("\n Error: GPIO 28 value open.");
        return -1;
    }

    fd29 = open("/sys/class/gpio/gpio29/value", O_WRONLY);
    if (fd29 < 0)
    {
        printf("\n Error: GPIO 29 value open.");
        return -1;
    }

    fd45 = open("/sys/class/gpio/gpio45/value", O_WRONLY);
    if (fd45 < 0)
    {
        printf("\n Error: GPIO 45 value open.");
        return -1;
    }
  

    //Write 1 to GPIO 28 and 0 to GPIOs 29 and 45
    if(write(fd28,"1", 1) < 0)
    {
        printf("\n Error: GPIO 28 value.");
        return -1;   
    }

    if(write(fd29,"0", 1) < 0)
    {
        printf("\n Error: GPIO 29 value.");
        return -1;   
    }

    if(write(fd45,"0", 1) < 0)
    {
        printf("\n Error: GPIO 45 value.");
        return -1;   
    }

    close(fd28);
    close(fd29);
    close(fd45);

    return 0;
}


//Function to set the mode of the specified device
void set_mode(int fd, int mode, int param)
{
    int ret;
    unsigned int margs[2];

    margs[0] = mode; //Set the mode
    margs[1] = param; //Set the frequency/buffer clear

    //Execute the IOCTL command to set the mode and frequency
    ret = ioctl(fd, SETMODE, margs);
    if (ret == 0)
    {
        if (mode == PERIODIC)
            printf(KCYAN "\n SETMODE - Periodic, Frequency: %d"RESET, margs[1]);
        else if (mode == ONESHOT)
            printf(KCYAN "\n SETMODE - Oneshot, Clear Buffer (Yes - 0, No - 1): %d"RESET, margs[1]);
    }
    else
    {
        printf(KRED"\n Error: Unable to SETMODE"RESET);
    }
}

//Function to set the echo and trigger pins
void set_pins(int fd, int echo, int trigger)
{
    int ret;
    unsigned int margs[2];

    margs[0] = echo; //Set the ECHO pin
    margs[1] = trigger; //Set the TRIGGER

    //Execute the IOCTL command to set the pins
    ret = ioctl(fd, SETPINS, margs);
    if (ret == 0)
    {
        printf(KCYAN "\n SETPINS - Echo: IO%d, Trigger: IO%d"RESET, margs[0], margs[1]);
    }
    else
    {
        printf(KRED"\n Error: Unable to SETPINS"RESET);
    }

}

void initial_test(int fd, char* name)
{
    int i;
    int ret;

    //Variables to store the write argument and the read value
    int warg = 0, rarg = 0;

    printf(BOLD KGRN"\n\n -->> Testing %s <<--\n"RESET, name);

    //PERIODIC MODE TEST
    printf(KGRN"\n ----- Testing Periodic Mode -----\n"RESET);

    //Set the mode as periodic and the frequency as 16 Hz
    set_mode(fd, PERIODIC, 16);

    //Start periodic mode
    warg = START_PERIODIC;
    ret = write(fd, &warg, sizeof(int));
    if (ret == 0)
        printf(KCYAN "\n %s: Started periodic mode."RESET, name);
    else
        printf(KRED"\n %s: Error: Unable to start periodic mode"RESET, name);
    

    //Read 10 measurement values
    for(i = 0; i < 10; i++)
    {
        //Attempt to read a measurement from the per-device FIFO buffer
        ret = read(fd, &rarg, sizeof(int));
        if (ret < 0)
            printf(KRED"\n %s: Error reading in periodic mode."RESET, name);
        else
            printf(KYEL "\n %s: Periodic - Distance reading: %d cm", name, rarg);
    }

    //Stop periodic mode
    warg = STOP_PERIODIC;
    ret = write(fd, &warg, sizeof(int));
    if (ret == 0)
        printf(KCYAN "\n %s: Stopped periodic mode."RESET, name);
    else
        printf(KRED"\n %s: Error: Unable to stop periodic mode"RESET, name);
    


    //ONE-SHOT MODE TEST
    printf(KGRN"\n\n ----- Testing One-shot Mode -----"RESET);

    //Set the mode as one-shot and clear the per-device FIFO buffer
    set_mode(fd, ONESHOT, 1);

    //Attempt to read a measurement from the per-device FIFO buffer
    ret = read(fd, &rarg, sizeof(int));
    if ( ret < 0)
        printf(KRED"\n %s: Error reading in one-shot mode."RESET, name);
    else
        printf(KYEL "\n %s: One-shot - Distance reading: %d cm"RESET, name, rarg);

    //Wait for a second
    printf(KCYAN "\n Waiting 0.5 second..."RESET);
    usleep(500000);

    //Attempt to read a measurement from the per-device FIFO buffer
    ret = read(fd, &rarg, sizeof(int));
    if ( ret < 0)
        printf(KRED"\n %s: Error reading in one-shot mode."RESET, name);
    else
        printf(KYEL "\n %s: One-shot - Distance reading: %d cm"RESET, name, rarg);
}

void combined_test_same(int fd1, int fd2, char *name1, char *name2)
{
    int i;
    int ret;

    //Variables to store the write argument and the read value
    int warg = 0, rarg = 0;

    printf(BOLD KGRN"\n\n -->> Testing %s and %s simultaneously - Periodic <<--\n"RESET, name1, name2);

    //Set the mode as periodic and the frequency as 16 Hz
    set_mode(fd1, PERIODIC, 16);

    //Set the mode as periodic and the frequency as 16 Hz
    set_mode(fd2, PERIODIC, 16);

    //Start periodic mode
    warg = START_PERIODIC;
    ret = write(fd1, &warg, sizeof(int));
    if (ret == 0)
        printf(KCYAN "\n\n %s: Started periodic mode."RESET, name1);
    else
        printf(KRED"\n %s: Error: Unable to start periodic mode"RESET, name1);

    //Start periodic mode
    warg = START_PERIODIC;
    ret = write(fd2, &warg, sizeof(int));
    if (ret == 0)
        printf(KCYAN "\n %s: Started periodic mode."RESET, name2);
    else
        printf(KRED"\n %s: Error: Unable to start periodic mode"RESET, name2);
    
    

    //Read 10 measurement values
    for(i = 0; i < 10; i++)
    {
        //Attempt to read a measurement from the per-device FIFO buffer
        ret = read(fd1, &rarg, sizeof(int));
        if (ret < 0)
            printf(KRED"\n %s: Error reading in periodic mode."RESET, name1);
        else
            printf(KYEL "\n %s: Periodic - Distance reading: %d cm", name1, rarg);

        //Attempt to read a measurement from the per-device FIFO buffer
        ret = read(fd2, &rarg, sizeof(int));
        if (ret < 0)
            printf(KRED"\n %s: Error reading in periodic mode."RESET, name2);
        else
            printf(KYEL "\n %s: Periodic - Distance reading: %d cm", name2, rarg);

        printf("\n");
    }

    //Stop periodic mode
    warg = STOP_PERIODIC;
    ret = write(fd1, &warg, sizeof(int));
    if (ret == 0)
        printf(KCYAN "\n %s: Stopped periodic mode."RESET, name1);
    else
        printf(KRED"\n %s: Error: Unable to stop periodic mode"RESET, name2);

    //Stop periodic mode
    warg = STOP_PERIODIC;
    ret = write(fd2, &warg, sizeof(int));
    if (ret == 0)
        printf(KCYAN "\n %s: Stopped periodic mode."RESET, name2);
    else
        printf(KRED"\n %s: Error: Unable to stop periodic mode"RESET, name2);
    


    //ONE-SHOT MODE TEST
    printf(BOLD KGRN"\n\n -->> Testing %s and %s simultaneously - One-shot <<--\n"RESET, name1, name2);


    //Set the mode as one-shot and clear the per-device FIFO buffer
    set_mode(fd1, ONESHOT, 1);

    //Set the mode as one-shot and clear the per-device FIFO buffer
    set_mode(fd2, ONESHOT, 1);

    //Attempt to read a measurement from the per-device FIFO buffer
    ret = read(fd1, &rarg, sizeof(int));
    if ( ret < 0)
        printf(KRED"\n %s: Error reading in one-shot mode."RESET, name1);
    else
        printf(KYEL "\n %s: One-shot - Distance reading: %d cm"RESET, name1, rarg);

    //Attempt to read a measurement from the per-device FIFO buffer
    ret = read(fd2, &rarg, sizeof(int));
    if ( ret < 0)
        printf(KRED"\n %s: Error reading in one-shot mode."RESET, name2);
    else
        printf(KYEL "\n %s: One-shot - Distance reading: %d cm"RESET, name2, rarg);

    //Wait for a second
    printf(KCYAN "\n Waiting 0.5 second..."RESET);
    usleep(500000);

    //Attempt to read a measurement from the per-device FIFO buffer
    ret = read(fd1, &rarg, sizeof(int));
    if ( ret < 0)
        printf(KRED"\n %s: Error reading in one-shot mode."RESET, name1);
    else
        printf(KYEL "\n %s: One-shot - Distance reading: %d cm"RESET, name1, rarg);

    //Attempt to read a measurement from the per-device FIFO buffer
    ret = read(fd2, &rarg, sizeof(int));
    if ( ret < 0)
        printf(KRED"\n %s: Error reading in one-shot mode."RESET, name2);
    else
        printf(KYEL "\n %s: One-shot - Distance reading: %d cm"RESET, name2, rarg);
}

void combined_test_complementry(int fd1, int fd2, char *name1, char *name2)
{
    int i;
    int ret;

    //Variables to store the write argument and the read value
    int warg = 0, rarg = 0;

    printf(BOLD KGRN"\n\n -->> Testing %s Periodic and %s One-shot Simultaneously <<--\n"RESET, name1, name2);

    //Set the mode as periodic and the frequency as 16 Hz
    set_mode(fd1, PERIODIC, 16);

    //Set the mode as periodic and clear the buffer
    set_mode(fd2, ONESHOT, 1);

    //Start periodic mode
    warg = START_PERIODIC;
    ret = write(fd1, &warg, sizeof(int));
    if (ret == 0)
        printf(KCYAN "\n\n %s: Started periodic mode."RESET, name1);
    else
        printf(KRED"\n %s: Error: Unable to start periodic mode"RESET, name1);

    //Read 10 measurement values
    for(i = 0; i < 10; i++)
    {
        //Attempt to read a measurement from the per-device FIFO buffer
        ret = read(fd1, &rarg, sizeof(int));
        if (ret < 0)
            printf(KRED"\n %s: Error reading in periodic mode."RESET, name1);
        else
            printf(KYEL "\n %s: Periodic - Distance reading: %d cm", name1, rarg);

        //Attempt to read a measurement from the per-device FIFO buffer
        ret = read(fd2, &rarg, sizeof(int));
        if ( ret < 0)
            printf(KRED"\n %s: Error reading in one-shot mode."RESET, name2);
        else
            printf(KYEL "\n %s: One-shot - Distance reading: %d cm"RESET, name2, rarg);

        printf("\n");
    }

    //Stop periodic mode
    warg = STOP_PERIODIC;
    ret = write(fd1, &warg, sizeof(int));
    if (ret == 0)
        printf(KCYAN "\n %s: Stopped periodic mode."RESET, name1);
    else
        printf(KRED"\n %s: Error: Unable to stop periodic mode"RESET, name2);


    printf(BOLD KGRN"\n\n -->> Testing %s Periodic and %s One-shot Simultaneously <<--\n"RESET, name2, name1);

    //Set the mode as periodic and the frequency as 16 Hz
    set_mode(fd2, PERIODIC, 16);

    //Set the mode as periodic and clear the buffer
    set_mode(fd1, ONESHOT, 1);

    //Start periodic mode
    warg = START_PERIODIC;
    ret = write(fd2, &warg, sizeof(int));
    if (ret == 0)
        printf(KCYAN "\n\n %s: Started periodic mode."RESET, name2);
    else
        printf(KRED"\n %s: Error: Unable to start periodic mode"RESET, name2);

    //Read 10 measurement values
    for(i = 0; i < 10; i++)
    {
        //Attempt to read a measurement from the per-device FIFO buffer
        ret = read(fd2, &rarg, sizeof(int));
        if (ret < 0)
            printf(KRED"\n %s: Error reading in periodic mode."RESET, name2);
        else
            printf(KYEL "\n %s: Periodic - Distance reading: %d cm", name2, rarg);

        //Attempt to read a measurement from the per-device FIFO buffer
        ret = read(fd1, &rarg, sizeof(int));
        if ( ret < 0)
            printf(KRED"\n %s: Error reading in one-shot mode."RESET, name1);
        else
            printf(KYEL "\n %s: One-shot - Distance reading: %d cm"RESET, name1, rarg);

        printf("\n");
    }

    //Stop periodic mode
    warg = STOP_PERIODIC;
    ret = write(fd2, &warg, sizeof(int));
    if (ret == 0)
        printf(KCYAN "\n %s: Stopped periodic mode."RESET, name2);
    else
        printf(KRED"\n %s: Error: Unable to stop periodic mode"RESET, name2);
   
}

int main()
{
    //int ret;

    //fd1 corresponds to HCSR_1 and fd2 corresponds to HCSR_2
    unsigned int fd1, fd2;

    //Set up the MUX, level shifter and pull-up/down GPIOs for both HCSR_1 and HCSR_2
    setup_pins_1();
    setup_pins_2();

    //Open HCSR_1 device
    fd1 = open("/dev/HCSR_1", O_RDWR);
    if (fd1 < 0 )
    {
        printf(KRED"\n - Can not open HCSR_1 file.\n"RESET);
        return 0;
    }

    //Open HCSR_2 device
    fd2 = open("/dev/HCSR_2", O_RDWR);
    if (fd2 < 0 )
    {
        printf(KRED"\n - Can not open HCSR_2 file.\n"RESET);
        return 0;
    }

    //Set the echo pin as IO2 and trigger pin as IO7 for HCSR_1
    set_pins(fd1, 2, 7);

    //Set the echo pin as IO1 and trigger pin as IO8 for HCSR_1
    set_pins(fd2, 1, 8);

    //Testing HCSR_1 -----------------------------------------------
    
    //Test periodic and one-shot mode one by one
    initial_test(fd1, "HCSR_1");

    printf("\n\n");

    //Testing HCSR_2 -----------------------------------------------
    
     //Test periodic and one-shot mode one by one
    initial_test(fd2, "HCSR_2");

    printf("\n\n");

    //Test periodic mode on both devices at the same time and one-shot mode on both devices at the same time
    combined_test_same(fd1, fd2, "HCSR_1", "HCSR_2");

    //Test perodic mode and one-shot mode together on one device first and vice versas
    combined_test_complementry(fd1, fd2, "HCSR_1", "HCSR_2");
    

    close(fd1);
    close(fd2);

    cleanup_pins_1();
    cleanup_pins_2();

    return 0;

}