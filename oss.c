/**
 * Author: Taylor Freiner
 * Date: November 7th, 2017
 * Log: Adding process structure 
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
#include "rstruct.h"
#include "pstruct.h"

#define BIT_COUNT 32

struct sembuf sb;
int sharedmem[5];
int processIds[100];
int globalProcessCount = 0;

void checkDeadlock();

bool req_lt_avail(const int*, const int*, const int, const int);

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
	shmctl(sharedmem[3], IPC_RMID, NULL);
	shmctl(sharedmem[4], IPC_RMID, NULL);

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
	int i, j, option;
	int processCount = 0;
	int bitArray[1] = { 0 };
	rStruct* rBlock;
	pStruct* pBlock;
	bool tableFull = 0;
	bool verbose = 0;
	int grantedRequests = 0;
	int lineCount = 0;

	//SIGNAL HANDLING
	signal(SIGINT, clean);
	signal(SIGALRM, clean);

	alarm(2);

	//FILE MANAGEMENT
	FILE *file = fopen("log.txt", "w");
	
	if(file == NULL){
		printf("%s: ", argv[0]);
		perror("Error: \n");
		exit(1);
	}

	//OPTIONS**********
	if (argc != 1 && argc != 2){
		fprintf(stderr, "%s Error: Incorrect number of arguments\n", argv[0]);
		exit(1);
	}
	while ((option = getopt(argc, argv, "hv")) != -1){
		switch (option){
			case 'h':
				printf("Usage: %s <-v>\n", argv[0]);
				printf("\t-v: verbose logging on\n");
				return 0;
				break;
			case 'v':
				verbose = true;
				break;
			case '?':
				fprintf(stderr, "%s Error: Usage: %s <-v>\n", argv[0], argv[0]);
				exi(1);
				break;
		}
	}
	//**********OPTIONS

	//SHARED MEMORY**********
	key_t key = ftok("keygen", 1);
	key_t key2 = ftok("keygen2", 1);
	key_t key3 = ftok("keygen3", 1);
	key_t key4 = ftok("keygen4", 1);
	key_t key5 = ftok("keygen5", 1);
	int memid = shmget(key, sizeof(int*)*2, IPC_CREAT | 0644);
	int memid2 = shmget(key2, sizeof(struct rStruct) * 20, IPC_CREAT | 0644);
	int memid3 = shmget(key4, sizeof(int*)*3, IPC_CREAT | 0644);
	int memid4 = shmget(key5, sizeof(struct pStruct) * 18, IPC_CREAT | 0644);
	int semid = semget(key3, 1, IPC_CREAT | 0644);
	if(memid == -1 || memid2 == -1){
		printf("%s: ", argv[0]);
		perror("Error: \n");
	}
	sharedmem[0] = memid;
	sharedmem[1] = memid2;
	sharedmem[2] = semid;
	sharedmem[3] = memid3;
	sharedmem[4] = memid4;
	int *clock = (int *)shmat(memid, NULL, 0);
	int *shmMsg = (int *)shmat(memid3, NULL, 0);
	rBlock = (struct rStruct *)shmat(memid2, NULL, 0);
	pBlock = (struct pStruct *)shmat(memid4, NULL, 0);
	if(*clock == -1 || (int*)rBlock == (int*)-1 || (int*)pBlock == (int*)-1){
		printf("%s: ", argv[0]);
		perror("Error: \n");
		clean(1);
	}
	int clockVal = 0;
	for(i = 0; i < 3; i++){
		if(i != 2)
			memcpy(&clock[i], &clockVal, 4);
		memcpy(&shmMsg[i], &clockVal, 4);
	}
	
	semctl(semid, 0, SETVAL, 1, arg);
	sb.sem_op = 1;
	sb.sem_num = 0;
	sb.sem_flg = 0;
	semop(semid, &sb, 1);
	if(errno){
		fprintf(stderr, "%s\n", strerror(errno));
		clean(1);
	}
	//**********SHARED MEMORY

	//CREATING PROCESSES
	int forkTime;
	int incrementTime;
	int lastForkTime[2];
	//int lastClockTime[2];
	int processIndex = 0;
	int totalProcessNum = 0;
	int percentShared = 0;
	//int initInstance[20];
	int clearProcess[18];
	
	lastForkTime[0] = clock[0];
	lastForkTime[1] = clock[1];
	//lastClockTime[0] = clock[0];
	//lastClockTime[1] = clock[1];
	pid_t childpid;
	srand(time(NULL));
	forkTime = rand() % 500000000 + 1;
	percentShared = rand() % 10 + 15;
	for(i = 0; i < 20; i++){
		rBlock[i].num = rand() % 10 + 1;
		if(i <= 20 * (double)percentShared / 100){
			rBlock[i].shared = true;
		}else{
			rBlock[i].shared = false;
		}
		if(i < 18)
			clearProcess[i] = 0;
		for(j = 0; j < 20; j++){
			if(i > 17)
				break;
			pBlock[i].resourceNum[j] = 0;
		}
	}

	while(clock[0] < 100 && clock[1] < 500000000){
		incrementTime = rand() % 1000;
		clock[0] += 1;
		if(clock[1] + incrementTime >= 1000000000){
			clock[1] = (clock[1] + incrementTime) % 1000000000;
			clock[0]++;
		}
		else
			clock[1] += incrementTime;
		if(processCount == 18){
			for(i = 0; i < 18; i++){
				if(clearProcess[i] == 1){
					unsetBit(bitArray, i);
					processCount--;
					processIndex = i;
					break;
				}
			}
		}
		if((clock[0] - lastForkTime[0]) >= 1 || (clock[0] == lastForkTime[0] && clock[1] - lastForkTime[1] > 500000000)){
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
					pBlock[processIndex].pid = childpid;
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
		checkDeadlock();
	}

	sleep(10);
	fclose(file);
	clean(1);
	
	return 0;
}

void checkDeadlock(){
	return;
}

//DEADLOCK ALGORITHM FROM NOTES
bool deadlock(const int *available, const int m, const int n, const int *request, const int *allocated){
	int work[m]; // m resources
	bool finish[n]; // n processes
	int i, p;
	for (i = 0; i < m; work[i] = available[i++]);
	for (i = 0; i < n; finish[i++] = false);
	for (p = 0; p < n; p++){
		if (finish[p])
			continue;
		if (req_lt_avail(request, work, p, m)){
			finish[p] = true;
			for (i = 0; i < m; i++)
				work[i] += allocated[p*m+i];
			p = -1;
		}
	}

	for (p = 0; p < n; p++)
		if (!finish[p])
			break;
	return (p != n);
}

bool req_lt_avail(const int *req, const int *avail, const int pnum, const int num_res){
	int i;
	for (i = 0; i < num_res; i++)
		if (req[pnum*num_res+i] > avail[i])
			break;
	return (i == num_res);
}
