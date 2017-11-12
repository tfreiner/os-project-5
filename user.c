/**
 * Author: Taylor Freiner
 * Date: November 12th, 2017
 * Log: Signaling user process to terminate
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
#include "rstruct.h"
#include "pstruct.h"
#define BOUND 5

int main(int argc, char* argv[]){
	struct sembuf sb;
	rStruct* rBlock;
	pStruct* pBlock;
	srand(time(NULL) ^ (getpid()<<16));
	int bound = rand() % BOUND + 1;
	//int checkTime = 0;
	int startSec = 0;
	int index = atoi(argv[1]);
	bool claim = true;
	bool resourcePicked = false;
	int i;
	int resource;
	bool terminate = false;
	
	//SHARED MEMORY
	key_t key = ftok("keygen", 1);
	key_t key2 = ftok("keygen2", 1);
	key_t key3 = ftok("keygen3", 1);
	key_t key4 = ftok("keygen4", 1);
	key_t key5 = ftok("keygen5", 1);
	key_t key6 = ftok("keygen6", 1);
	int memid = shmget(key, sizeof(int*)*2, IPC_CREAT | 0644);
	int memid2 = shmget(key2, sizeof(struct rStruct) * 20, 0);
	int semid = semget(key3, 1, 0);
	int memid3 = shmget(key4, sizeof(int*)*3, IPC_CREAT | 0644);
	int memid4 = shmget(key5, sizeof(struct pStruct) * 18, 0);
	int memid5 = shmget(key6, sizeof(int*)*2, IPC_CREAT | 0644);
	int *clock = (int *)shmat(memid, NULL, 0);	
	int *shmMsg = (int *)shmat(memid3, NULL, 0);
	int *termMsg = (int *)shmat(memid5, NULL, 0);
	rBlock = (rStruct *)shmat(memid2, NULL, 0);
	pBlock = (pStruct *)shmat(memid4, NULL, 0);

	startSec = clock[0];

	//printf("CLOCK[0]: %d, BOUND: %d\n", clock[0], bound);
	while(1){
		//checkTime = rand() % 250000000;
		//if(clock[0] - startSec > bound){
		if(termMsg[0] == index && termMsg[1] == 1){ //check if process was terminated by the deadlock algorithm
		//	sb.sem_op = 1;
		//	sb.sem_num = 0;
		//	sb.sem_flg = 0;
		//	semop(semid, &sb, 1);
			printf("I AM: %d\n", getpid());
			termMsg[0] = -1;
			termMsg[1] = -1;
			exit(1);
		}
	
		if(terminate){
			sb.sem_op = -1;
			sb.sem_num = 0;
			sb.sem_flg = 0;
			semop(semid, &sb, 1);
			shmMsg[0] = index;
 			shmMsg[1] = 2; //terminate
			shmMsg[2] = resource;
		}else{
			resource = rand() % 20;
			if(pBlock[index].numClaimed == 0)
				claim = true;
			else
				claim = rand() % 2;

			if(claim){
				sb.sem_op = -1;
 				sb.sem_num = 0;
 				sb.sem_flg = 0;
				semop(semid, &sb, 1);
				shmMsg[0] = index;
				shmMsg[1] = 1; //claim
				shmMsg[2] = resource; //resource to claim.
			}else{
				resource = -1;
				for(i = 0; i < 20; i++){
					if(pBlock[index].resourceNum[i] > 0){
						resource = i;
						break;
					}
				}
				if(resource >= 0){
					sb.sem_op = -1;
 					sb.sem_num = 0;
 					sb.sem_flg = 0;
					semop(semid, &sb, 1);
					shmMsg[0] = index;
					shmMsg[1] = 0; //release
					shmMsg[2] = resource; //resource to release
				}
			}
	//		break;
		//}
		}
	}

	exit(0);
}
