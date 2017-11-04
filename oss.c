/**
 * Author: Taylor Freiner
 * Date: November 4th, 2017
 * Log: Copying over my project 4 
 */

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <getopt.h>
#include <ctype.h>
#include <signal.h>
#include <unistd.h>
#include <semaphore.h>
#include <stdbool.h>
#include <time.h>
#include <sys/shm.h>
#include <sys/sem.h>
#include <sys/wait.h>
#include <errno.h>
#include <sys/types.h>
#include <math.h>
#include "sys.h"

#define BIT_COUNT 32

struct sembuf sb;
int sharedmem[3];
int processIds[100];
int globalProcessCount = 0;

union semun{
	int val;
};

void clean(int sig){

	if(sig == 2)
		fprintf(stderr, "Interrupt signaled. Removing shared memory and killing processes.\n");
	else if(sig == 14)
		fprintf(stderr, "Program reached 2 seconds. Removing shared memory and killing processes.\n");	

	int i;
	shmctl(sharedmem[0], IPC_RMID, NULL);
	shmctl(sharedmem[1], IPC_RMID, NULL);
	semctl(sharedmem[2], 0, IPC_RMID);
	for(i = 0; i < globalProcessCount; i++){
		kill(processIds[i], SIGKILL);
	}
	exit(1);
}

void setBit(int bitArray[], int i){
	bitArray[i/BIT_COUNT] |= 1 << (i % BIT_COUNT);
}

void unsetBit(int bitArray[], int i){
	bitArray[i/BIT_COUNT] &= ~(1 << (i % BIT_COUNT));
}

bool checkBit(int bitArray[], int i){
	return ((bitArray[i/BIT_COUNT] & (1 << (i % BIT_COUNT))) != 0);
}

int main(int argc, char* argv[]){
	union semun arg;
	arg.val = 1;
	int i;
	int processCount = 0;
	int bitArray[1] = { 0 };
	sysStruct* sysBlock;
	bool tableFull = 0;

	//SIGNAL HANDLING
	signal(SIGINT, clean);
	signal(SIGALRM, clean);

	alarm(2);

	//FILE MANAGEMENT
	FILE *file = fopen("log.txt", "w");
	
	if(file == NULL){
		printf("%s: ", argv[0]);
		perror("Error: \n");
		return 1;
	}

	//SHARED MEMORY
	key_t key = ftok("keygen", 1);
	key_t key2 = ftok("keygen2", 1);
	key_t key3 = ftok("keygen3", 1);
	int memid = shmget(key, sizeof(int*)*2, IPC_CREAT | 0644);
	int memid2 = shmget(key2, sizeof(struct sysStruct) * 20, IPC_CREAT | 0644);
	int semid = semget(key3, 1, IPC_CREAT | 0644);
	if(memid == -1 || memid2 == -1){
		printf("%s: ", argv[0]);
		perror("Error: \n");
	}
	sharedmem[0] = memid;
	sharedmem[1] = memid2;
	sharedmem[2] = semid;
	int *clock = (int *)shmat(memid, NULL, 0);
	sysBlock = (struct sysStruct *)shmat(memid2, NULL, 0);
	if(*clock == -1 || (int*)sysBlock == (int*)-1){
		printf("%s: ", argv[0]);
		perror("Error: \n");
	}
	int clockVal = 0;
	for(i = 0; i < 2; i++){
		memcpy(&clock[i], &clockVal, 4);
	}
	
	semctl(semid, 0, SETVAL, 1, arg);
	sb.sem_op = 1;
	sb.sem_num = 0;
	sb.sem_flg = 0;
	semop(semid, &sb, 1);
	if(errno){
		fprintf(stderr, "%s\n", strerror(errno));
		exit(1);
	}

	//CREATING PROCESSES
	int forkTime;
	int incrementTime;
	int lastForkTime[2];
	int lastClockTime[2];
	int processIndex = 0;
	int totalProcessNum = 0;
	lastForkTime[0] = clock[0];
	lastForkTime[1] = clock[1];
	lastClockTime[0] = clock[0];
	lastClockTime[1] = clock[1];
	pid_t childpid;
	srand(time(NULL));
	forkTime = rand() % 500000001;
	while(1){
		incrementTime = rand() % 1000;
		clock[0] += 1;
		if(clock[1] + incrementTime >= 1000000000){
			clock[1] = (clock[1] + incrementTime) % 1000000000;
			clock[0]++;
		}
		else
			clock[1] += incrementTime;
		if(processCount == 18){
			for(i = 0 ; i < 18; i++){
			//	if(sysBlock[i].pid == -2 && processCount == 18){
					unsetBit(bitArray, i);
					processCount--;
					processIndex = i;
					break;
			//	}
			}
		}
		if(clock[0] - lastForkTime[0] > forkTime){
			lastForkTime[0] = clock[0];
			lastForkTime[1] = clock[1];
			forkTime = rand() % 3;
			for(i = 0; i < 18; i++){
				if(checkBit(bitArray, i) == 0){
					tableFull = 0;
					setBit(bitArray, i);
					break;
				}
				tableFull = 1;
			}
			if(!tableFull){
				childpid = fork();
				if(errno){
					fprintf(stderr, "%s", strerror(errno));
					clean(1);
				}

				if(childpid == 0){
					char arg[12];
					sprintf(arg, "%d", processIndex);
					execl("./user", "user", arg, NULL);
				}else{
					processIds[processIndex] = childpid;
					processCount++;
					globalProcessCount++;
					processIndex++;
					totalProcessNum++;
				}
				if(errno){
					fprintf(stderr, "%s\n", strerror(errno));
					exit(1);
				}
			}
		}

		if(clock[0] > 100){
			break;
		}
	}

	sleep(10);
	fclose(file);
	clean(1);
	
	return 0;
}
