/**
 * Author: Taylor Freiner
 * Date: November 5th, 2017
 * Log: Setting bound
 */

#include <stdio.h>
#include <stdlib.h>
#include <stdbool.h>
#include <unistd.h>
#include <time.h>
#include <semaphore.h>
#include <sys/sem.h>
#include <sys/shm.h>
#include <string.h>
#include "sys.h"
#define BOUND 100

int main(int argc, char* argv[]){
	struct sembuf sb;
	sysStruct* sysBlock;
	srand(time(NULL) ^ (getpid()<<16));
	int bound = rand() % BOUND;
	int checkTime = rand() % 250000000;
	
	//SHARED MEMORY
	key_t key = ftok("keygen", 1);
	key_t key2 = ftok("keygen2", 1);
	key_t key3 = ftok("keygen3", 1);
	key_t key4 = ftok("keygen4", 1);
	int memid = shmget(key, sizeof(int*)*2, IPC_CREAT | 0644);
	int memid2 = shmget(key2, sizeof(struct sysStruct) * 20, 0);
	int semid = semget(key3, 1, 0);
	int memid3 = shmget(key4, sizeof(int*)*2, IPC_CREAT | 0644);

	int *clock = (int *)shmat(memid, NULL, 0);	
	int *shmMsg = (int *)shmat(memid3, NULL, 0);
	sysBlock = (sysStruct *)shmat (memid2, NULL, 0);

	exit(0);
}
