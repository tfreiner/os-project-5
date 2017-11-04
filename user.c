/**
 * Author: Taylor Freiner
 * Date: November 4th, 2017
 * Log: Copying over my project 4
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

int main(int argc, char* argv[]){
	struct sembuf sb;
	srand(time(NULL) ^ (getpid()<<16));
	int quantumUse = rand() % 2;
	int quantumLength[2];
	quantumLength[0] = 0;
	quantumLength[1] = 0;
	int index = atoi(argv[1]);
	controlBlockStruct* controlBlock;
	bool ready = false;

	key_t key = ftok("keygen", 1);
	key_t key2 = ftok("keygen2", 1);
	key_t key3 = ftok("keygen3", 1);

	int memid = shmget(key, sizeof(int*)*2, IPC_CREAT | 0644);
	int memid2 = shmget(key2, sizeof(struct controlBlockStruct) * 19, 0);
	int semid = semget(key3, 1, 0);

	int *clock = (int *)shmat(memid, NULL, 0);	
	controlBlock = (controlBlockStruct *)shmat (memid2, NULL, 0);

	while(!ready){
		if(controlBlock[index].ready){
			ready = true;
		}
	}

	if(controlBlock[index].task == 1){
		if(quantumUse == 1){
			quantumLength[0] = rand() % controlBlock[index].quantum[0] + 1;
			quantumLength[1] = rand() % controlBlock[index].quantum[1] + 1;
		}else{
			quantumLength[0] = controlBlock[index].quantum[0];
			quantumLength[1] = controlBlock[index].quantum[1];
		}
	}else if(controlBlock[index].task == 3){
		if(quantumUse == 1){
			quantumLength[0] = rand() % controlBlock[index].quantum[0] + 1;
			quantumLength[1] = rand() % controlBlock[index].quantum[1] + 1;
		}else{
			quantumLength[0] = controlBlock[index].quantum[0];
			quantumLength[1] = controlBlock[index].quantum[1];
		}
		quantumLength[0] = quantumLength[0] * (controlBlock[index].p/100);
		quantumLength[1] = quantumLength[1] * (controlBlock[index].p/100);
	}else if(controlBlock[index].task == 2){
		quantumLength[0] = controlBlock[index].quantum[0];
		quantumLength[1] = controlBlock[index].quantum[1];
	}
	clock[0] += quantumLength[0];
	if(clock[1] + quantumLength[1] >= 1000000000){
		clock[1] = (clock[1] + quantumLength[1]) % 1000000000;
		clock[0]++;
	}else{
		clock[1] += quantumLength[1];
	}
	controlBlock[index].quantum[0] = quantumLength[0];
	controlBlock[index].quantum[1] = quantumLength[1];

	controlBlock[index].ready = false;
	sb.sem_op = 1;
	sb.sem_num = 0;
	sb.sem_flg = 0;
	semop(semid, &sb, 1);
	
	exit(0);
}
