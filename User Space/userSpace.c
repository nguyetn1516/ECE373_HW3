/* 
userSpace.c

Description: This is the user space file for ECE 373 Assignment 3.
            It opens /dev/LEDkernel, reads, and writes to LED register
            to turn on LED0 and turn it off after 2 seconds.
            
Natalie Nguyen
ECE 373 - Assignment 3
05/05/2020
*/

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/file.h>
#include <sys/types.h>
#include <sys/stat.h>
#include <unistd.h>
#include <fcntl.h>

int main(int argc, const char *argv[])
{
	int fd;			// file descriptor	
	u_int32_t buffer;

	fd = open("/dev/LEDkernel", O_RDWR);	// open /dev/deviceNode

	/* If file descriptor < 0 then failed */
	if (fd<0)
	{	
		write(STDERR_FILENO, "Error: Could not open file!\n", 28);
		exit(0);
	}

	/* else opened successfully */
	else
	{	write(STDOUT_FILENO, "Opened file successfully!\n", 26); 

		read(fd, &buffer, sizeof(u_int32_t));					// read from LED register
		printf("The current value of LED register: %x\n", buffer);
		printf("Writing to LED0 to turn it on ... \n");

        buffer = buffer & 0xFFFFFFF0 | 0x0000000E;              // keep the current settings and turn on LED0, RMW

        if(write(fd, &buffer, sizeof(u_int32_t)) == -1)         // write to LED register to turn on LED0
        {
            write(STDERR_FILENO, "Error: Could not write to device to turn on LED0!\n", 50);
            exit(0);
        }

        write(STDOUT_FILENO, "Wrote to device successfully to turn on LED0!\n", 46);
        read(fd, &buffer, sizeof(u_int32_t));                   // reaf from LED register again
        printf("The value of LED0 is: %x\n", buffer);

        sleep(2);                                               // sleep for 2 seconds

        buffer = buffer & 0xFFFFFFF0 | 0x0000000F;              // keep the current settings and turn off LED0

        if(write(fd, &buffer, sizeof(u_int32_t)) == -1)         // write to LED register to turn off LED0
        {
            write(STDERR_FILENO, "Error: Could not write to device to turn off LED0!\n", 51);
            exit(0);
        }
        write(STDOUT_FILENO, "Wrote to device successfully to turn off LED0!\n", 47);
    }

	close(fd);      // close file descriptor
	return 0;
}
