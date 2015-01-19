/*
Date: 4/23/2014
Author: Jake Crane
Description: This is my CS4500 Term Project. I chose to do the Fraternity Party project. This program simulates partiers drinking from a communal keg. The parent process represents a pledge. The child processes represent partiers. Semaphores are used to keep track of information about the keg. The pledge sleeps at the party until one of partiers notices that the keg is empty and wakes the pledge up. When the pledge wakes up he refills the keg and goes back to sleep. When the pledge is sleeping, the process that represents the pledge is waiting on a semaphore. This program calls the sleep function so the user is able to easily read all of the output. This program will run until it is signaled to stop. When the user presses ctrl c to send the interupt signal, the program will wait for all child processes to end, destroy all of the semaphores and unlink the shared memory.

Compile Information: This program must be compiled with gcc arguments -lrt and -pthread ex: gcc party.c -pthread -lrt.
Run information: This program requires one command line argument. The argument is an integer that specifies how many servings the keg can hold.
*/

#include <stdio.h>
#include <fcntl.h>
#include <sys/types.h>
#include <sys/mman.h>
#include <sys/shm.h>
#include <sys/stat.h>
#include <semaphore.h>
#include <unistd.h>
#include <signal.h>
#include <stdlib.h>
#include <termios.h>

#define NUMBER_OF_PARTIERS 10

const char *file_name = "1235498984948"; // may be located in /dev/shm on foley or /run/shm on other systems
sem_t* keg_avaliable = NULL;
sem_t* keg_servings = NULL;
sem_t* keg_empty = NULL;

/*This function is called when an error occurs. 
It makes sure that all semaphores are destroyed and all shared memory is unlinked.
*/
void error_cleanup_and_exit() {

	while(wait(NULL) != -1); //wait for all child processes to end


	if (keg_avaliable != NULL) {
		sem_destroy(keg_avaliable);
	}
	if (keg_servings != NULL) {
		sem_destroy(keg_servings);
	}
	if (keg_empty != NULL) {
		sem_destroy(keg_empty);
	}

	if (shm_unlink(file_name) == -1) {
		perror("error unlinking shared memory");
	}
	exit(1);
}

/*This function runs when the program is sent an interupt signal.
It makes sure that all semaphores are destroyed and all shared memory is unlinked.
*/
void cleanup_and_exit() {
	printf("\nstopping...\n");

	while(wait(NULL) != -1); //wait for all child processes to end

	if (sem_destroy(keg_avaliable) == -1) {
		perror("error destroying semaphore");
		error_cleanup_and_exit();
	}
	if (sem_destroy(keg_servings) == -1) {
		perror("error destroying semaphore");
		error_cleanup_and_exit();
	}
	if (sem_destroy(keg_empty) == -1) {
		perror("error destroying semaphore");
		error_cleanup_and_exit();
	}
	if (shm_unlink(file_name) == -1) {
		perror("error unlinking shared memory");
		exit(1);
	}
	exit(0);
}

int main(int argc, char* args[]) {

	int max_keg_servings;
	int pid;
	int shm_fd;

	setvbuf(stdout, NULL, _IONBF, 0); //set stdout unbuffered

	if (argc != 2) {
		printf("Usage: %s max_keg_servings\n", args[0]);
		return 1;
	}

	max_keg_servings = atoi(args[1]);
	if (max_keg_servings < 1) { //validate user input
		printf("Invalid maximum number of keg servings. Enter a number > 0.\n");
		return 1;
	}
	
	signal(SIGINT, cleanup_and_exit); //call cleanup_and_exit when the user presses ctrl c

	shm_fd = shm_open(file_name, O_CREAT|O_RDWR, 0666);

	if (shm_fd == -1) {
		perror("failed to open shared memory");
		return 1;
	}

	if (ftruncate(shm_fd, sizeof(sem_t) * 3) == -1) {
		perror("failed to ftruncate");
		error_cleanup_and_exit();
	}

	keg_avaliable = (sem_t *) mmap(0, sizeof(sem_t) * 3, PROT_READ | PROT_WRITE, MAP_SHARED, shm_fd, 0);
	if (keg_avaliable == MAP_FAILED) {
		perror("mmap failed");
		error_cleanup_and_exit();
	}
	keg_servings = keg_avaliable + 1;
	keg_empty = keg_avaliable + 2;


	if (sem_init(keg_avaliable, 1, 1) == -1) {
		perror("Failed to create semaphore");
		error_cleanup_and_exit();
	}

	if (sem_init(keg_servings, 1, max_keg_servings) == -1) {
		perror("Failed to create semaphore");
		error_cleanup_and_exit();
	}

	if (sem_init(keg_empty, 1, 0) == -1) {
		perror("Failed to create semaphore");
		error_cleanup_and_exit();
	}

	printf("This program should be stopped using ctrl c.\n");
	printf("Press Enter to start the program.");
	while (getchar() != '\n');//This waits for the user to press enter and ensures no input is left in the stream.
	printf("\n");

	int i;
	for (i = 0; i < NUMBER_OF_PARTIERS; i++) {
		if ((pid=fork())  == -1) {
			perror("Failed to create child process");
			error_cleanup_and_exit();
		}
		if (pid == 0) { //child process
			break;
		}
	}

	if (pid == 0) { //child process

		signal(SIGINT, SIG_DFL); //set child process's signal disposition back to default
	
		int mypid = getpid();

		while (1) {
			
			if (sem_wait(keg_avaliable) == -1) { //decrement
				perror("Failed to wait on keg_avaliable");
				error_cleanup_and_exit();
			}

			printf("partier %d is checking keg servings\n", mypid);

			int kegServings;
			if (sem_getvalue(keg_servings, &kegServings) == -1) {
				perror("Failed to get keg servings");
				error_cleanup_and_exit();
			}

			sleep(2); //sleep so the user has time to read the output

			printf("There are %d servings in the keg\n", kegServings);

			if (kegServings > 0) {
				printf("partier %d is filling his cup\n", mypid);
				if (sem_wait(keg_servings) == -1) { //decrement
					perror("Failed to wait on keg_servings");
					error_cleanup_and_exit();
				}
				sleep(5); //sleep so the user has time to read the output
				if (sem_post(keg_avaliable) == -1) { //increment
					perror("child Failed to post1 keg_avaliable");
					error_cleanup_and_exit();
				}
			} else {
				printf("The keg is empty, waking pledge to refill it\n");
				if (sem_post(keg_empty) == -1) { //increment
					perror("child Failed to post2 keg_empty");
					error_cleanup_and_exit();
				}
			}

			sleep(2); //sleep so the user has time to read the output

		}
	} else {

		while (1) {
		
			if (sem_wait(keg_empty) == -1) { //decrement
				perror("Failed to wait");
				error_cleanup_and_exit();
			}
	
			printf("The pledge is refilling the keg\n");
			int i;
			for (i = 0; i < max_keg_servings; i++) {
				if (sem_post(keg_servings) == -1) { //increment
					perror("parent Failed to post1 keg_servings");
					error_cleanup_and_exit();
				}
			}

			sleep(3); //sleep so the user has time to read the output

			printf("The keg has been refilled\n"); 
			printf("The pledge is going back to sleep\n"); 
			
			if (sem_post(keg_avaliable) == -1) { //increment
				perror("parent Failed to post2 keg_avaliable");
				error_cleanup_and_exit();
			}
			sleep(5); //sleep so the user has time to read the output
			
		}
		
	}
	
	return 0;

}
