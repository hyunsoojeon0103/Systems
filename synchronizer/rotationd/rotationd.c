/*
 * Columbia University
 * COMS W4118 Fall 2019
 * Homework 3 - rotationd.c
 * teamN: UNI, UNI, UNI
 */

#include <math.h>
#include <stdbool.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>

#include <hardware/hardware.h>
#include <hardware/sensors.h>

#include "rotationd.h"

#include <sys/types.h>
#include <sys/stat.h>

static int open_sensors(struct sensors_module_t **sensors,
			struct sensors_poll_device_t **device);
static int poll_sensor_data(struct sensors_poll_device_t *device,
			    struct dev_rotation *cur_rotation);

static void createDeamon(); 
int main(int argc, char **argv)
{
	struct sensors_module_t *sensors;
	struct sensors_poll_device_t *device;

	if (open_sensors(&sensors, &device))
		return EXIT_FAILURE;

	/********** Demo code begins **********/

	/* Replace demo code with your daemon */
	
	createDeamon();

	struct dev_rotation rotation;

	while (true) {
		if (poll_sensor_data(device, &rotation)) {
			printf("No data received!\n");
		} else {
			if(syscall(333,&rotation) != 0)
				printf("ERROR!\n");
			usleep(1000);
		}
	}
	/*********** Demo code ends ***********/

	return EXIT_SUCCESS;
}
static void createDeamon()
{
	/* daemon creating */
	pid_t pid, sid;
	
	/* Fork off the parent process */
	pid = fork();
	if (pid < 0) {
		/* Log failure (use syslog if possible) */
		exit(EXIT_FAILURE);
	}
	/* 
	 * got a good PID, then we can exit
	 * the parent process
	 */
	if (pid > 0)
		exit(EXIT_SUCCESS);
	
	/* 
	 * By setting the umask to 0, we will have full access 
	 * to the files generated by the daemon
	 */
	umask(0);
	
	/* 
	 * Create a new SID for the child process to prevent
  	 * it from becoming an orphan
	 */	
	 
	sid = setsid();
	if (sid < 0 ) {
		/* log any failure if possible */
		exit(EXIT_FAILURE);
	}

	/*
	 * change the current working directory to
	 * some place guranteed to always be there
	 * since many Linux distros do not completely follow
	 * the Linux Filesystem Hierachy standard
	 */
	if ((chdir("/")) < 0 ) { 
		/* defensive coding, chdir return -1 on failure */
		exit(EXIT_FAILURE);	
	}

	/*
	 * Since a daemon cannot use the terminal, close the redundant
	 * file descriptors which are a potential security hazard
	 * Also, for the greatest portability between system versions
	 * it is a good idea to stick with the constants defined for
	 * the descriptors
	 */
	close(STDIN_FILENO);
	close(STDOUT_FILENO);
	close(STDERR_FILENO);
	
	
	/* daemon created */

} 
/***************************** DO NOT TOUCH BELOW *****************************/

static inline int open_sensors(struct sensors_module_t **sensors,
			       struct sensors_poll_device_t **device)
{
	int err = hw_get_module("sensors",
				(const struct hw_module_t **)sensors);

	if (err) {
		fprintf(stderr, "Failed to load %s module: %s\n",
			SENSORS_HARDWARE_MODULE_ID, strerror(-err));
		return -1;
	}

	err = sensors_open(&(*sensors)->common, device);

	if (err) {
		fprintf(stderr, "Failed to open device for module %s: %s\n",
			SENSORS_HARDWARE_MODULE_ID, strerror(-err));
		return -1;
	}

	if (!*device)
		return -1;

	const struct sensor_t *list;
	int count = (*sensors)->get_sensors_list(*sensors, &list);

	if (count < 1 || !list) {
		fprintf(stderr, "No sensors!\n");
		return -1;
	}

	for (int i = 0 ; i < count ; ++i) {
		printf("%s (%s) v%d\n", list[i].name, list[i].vendor,
			list[i].version);
		printf("\tHandle:%d, type:%d, max:%0.2f, resolution:%0.2f\n",
			list[i].handle, list[i].type, list[i].maxRange,
			list[i].resolution);
		(*device)->activate(*device, list[i].handle, 1);
	}
	return 0;
}

static int poll_sensor_data(struct sensors_poll_device_t *device,
			    struct dev_rotation *cur_rotation)
{
	static const size_t event_buffer_size = 128;
	sensors_event_t buffer[event_buffer_size];

	int count = device->poll(device, buffer, event_buffer_size);

	for (int i = 0; i < count; ++i) {
		if (buffer[i].type != SENSOR_TYPE_GYROSCOPE)
			continue;
		cur_rotation->x =
			(int)((buffer[i].gyro.x) * 60);
		cur_rotation->y =
			(int)((buffer[i].gyro.y) * 60);
		cur_rotation->z =
			(int)((buffer[i].gyro.z) * 60);
		return 0;
	}

	return -1;
}