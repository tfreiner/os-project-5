/**
 * Author: Taylor Freiner
 * Date: November 14th, 2017
 * Log: Fixing several bugs 
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
int sharedmem[6];
int processIds[1000];
int globalProcessCount = 0;
bool verbose = 0;
int grantedRequests = 0;
int lineCount = 0;
int deadlockedProcesses[18];
int clearProcess[18];
int deadlockCount = 0;
bool wasDeadlock = false;
int grantedRequestsGlobal = 0;
int killedProcesses = 0;
int terminatedProcesses = 0;
int processCount = 0;
int deadAlgRun = 0;
int deadlockFixed = 0;
int availableVector[20];
int requestMatrix[18][20];
int allocatedMatrix[18][20];

void checkRequests(int*, int*, pStruct*, rStruct*, int*, FILE*);

bool deadlock(const int*, const int m, const int n, int[18][20], int[18][20], FILE*, int);

bool req_lt_avail(int[18][20], const int*, const int, const int);

union semun{
	int val;
};

void clean(int sig){

	if(sig == 2)
		fprintf(stderr, "Interrupt signaled. Removing shared memory and killing processes.\n");
	else if(sig == 14)
		fprintf(stderr, "Program reached 2 seconds. Removing shared memory and killing processes.\n");	
	else if(sig == 11)
		fprintf(stderr, "Seg fault caught. Removing shared memory and killing processes. Please re-run program.\n");

	int i;
	shmctl(sharedmem[0], IPC_RMID, NULL);
	shmctl(sharedmem[1], IPC_RMID, NULL);
	semctl(sharedmem[2], 0, IPC_RMID);
	shmctl(sharedmem[3], IPC_RMID, NULL);
	shmctl(sharedmem[4], IPC_RMID, NULL);
	shmctl(sharedmem[5], IPC_RMID, NULL);

	for(i = 0; i < globalProcessCount; i++){
		kill(processIds[i], SIGKILL);
	}

	printf("Number of granted requests: %d\n", grantedRequestsGlobal);
	printf("Number of processes killed by the deadlock detection algorithm: %d\n", killedProcesses);
	printf("Number of processes that terminated successfully: %d\n", terminatedProcesses);
	printf("Number of times the deadlock detection algorithm ran: %d\n", deadAlgRun);
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
	int bitArray[1] = { 0 };
	rStruct* rBlock;
	pStruct* pBlock;
	bool tableFull = 0;

	//SIGNAL HANDLING
	signal(SIGINT, clean);
	signal(SIGALRM, clean);
	signal(SIGSEGV, clean);

	alarm(2);

	//FILE MANAGEMENT
	FILE *file = fopen("log.txt", "w");
	
	if(file == NULL){
		fprintf(stderr, "%s: ", argv[0]);
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
				exit(1);
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
	key_t key6 = ftok("keygen6", 1);
	int memid = shmget(key, sizeof(int*)*2, IPC_CREAT | 0644);
	int memid2 = shmget(key2, sizeof(struct rStruct) * 20, IPC_CREAT | 0644);
	int memid3 = shmget(key4, sizeof(int*)*3, IPC_CREAT | 0644);
	int memid4 = shmget(key5, sizeof(struct pStruct) * 18, IPC_CREAT | 0644);
	int memid5 = shmget(key6, sizeof(int*)*2, IPC_CREAT | 0644);
	int semid = semget(key3, 1, IPC_CREAT | 0644);
	if(memid == -1 || memid2 == -1){
		fprintf(stderr, "%s: ", argv[0]);
		perror("Error: \n");
	}
	sharedmem[0] = memid;
	sharedmem[1] = memid2;
	sharedmem[2] = semid;
	sharedmem[3] = memid3;
	sharedmem[4] = memid4;
	sharedmem[5] = memid5;
	int *clock = (int *)shmat(memid, NULL, 0);
	int *shmMsg = (int *)shmat(memid3, NULL, 0);
	int *termMsg = (int *)shmat(memid5, NULL, 0);
	rBlock = (struct rStruct *)shmat(memid2, NULL, 0);
	pBlock = (struct pStruct *)shmat(memid4, NULL, 0);
	if(*clock == -1 || *termMsg == -1 || (int*)rBlock == (int*)-1 || (int*)pBlock == (int*)-1){
		fprintf(stderr, "%s: ", argv[0]);
		perror("Error: \n");
		clean(1);
	}
	int clockVal = 0;
	int messageVal = -1;
	for(i = 0; i < 3; i++){
		if(i != 2){
			memcpy(&clock[i], &clockVal, 4);
			memcpy(&termMsg[i], &clockVal, 4);
		}
		memcpy(&shmMsg[i], &messageVal, 4);
	}
	
	semctl(semid, 0, SETVAL, 1, arg);
	sb.sem_op = 1;
	sb.sem_num = 0;
	sb.sem_flg = 0;
	semop(semid, &sb, 1);
	if(errno){
		fprintf(stderr, "....%s\n", strerror(errno));
		clean(1);
	}
	//**********SHARED MEMORY
	//CREATING PROCESSES
	///int forkTime;
	int incrementTime;
	int lastForkTime[2];
	//int lastClockTime[2];
	int processIndex = 0;
	int totalProcessNum = 0;
	int percentShared = 0;
	//int initInstance[20];
	deadlockCount = 0;	
	lastForkTime[0] = clock[0];
	lastForkTime[1] = clock[1];
	//lastClockTime[0] = clock[0];
	//lastClockTime[1] = clock[1];
	pid_t childpid;
	srand(time(NULL));
	//forkTime = rand() % 500000000 + 1;
	percentShared = rand() % 10 + 15;

	for(i = 0; i < 18; i++){
		for(j = 0; j < 20; j++){
			requestMatrix[i][j] = 0;
			allocatedMatrix[i][j] = 0;
		}
	}

	for(i = 0; i < 20; i++){
		rBlock[i].num = rand() % 10 + 1;
		if(i <= 20 * (double)percentShared / 100){ //shared processes will always be in the front of the resource block
			rBlock[i].shared = true;
		}else{
			rBlock[i].shared = false;
		}
		if(i < 18){
			clearProcess[i] = 0;
			deadlockedProcesses[i] = 0;
		}
		for(j = 0; j < 20; j++){
			if(i > 17)
				break;
			pBlock[i].resourceNum[j] = 0;
		}
		availableVector[i] = rBlock[i].num;
	}

	int processCountHere = 0;
	//clock[0] < 500 && clock[1] < 500000000
	while(deadlockFixed < 5){
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
					for(j = 0; j < 18; j++){
						clearProcess[j] = 0;
					}
					unsetBit(bitArray, i);
					processCount--;
					processIndex = i;
					clearProcess[i] = 0;
					break;
				}
			}
		}
		if((clock[0] - lastForkTime[0]) >= 1 || (clock[0] == lastForkTime[0] && clock[1] - lastForkTime[1] > 500000000)){
			lastForkTime[0] = clock[0];
			lastForkTime[1] = clock[1];
			//forkTime = rand() % 3;
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
				
				processCountHere++;
				if(errno){
					fprintf(stderr, "!!!%s", strerror(errno));
					clean(1);
				}
				if(childpid == 0){
					char arg[12];
					sprintf(arg, "%d", processIndex);
					execl("./user", "user", arg, NULL);
				}else{
					processIds[globalProcessCount] = childpid;
					pBlock[processIndex].pid = childpid;
					processCount++;
					globalProcessCount++;
					processIndex++;
					totalProcessNum++;
				}
				if(errno){
					fprintf(stderr, "...%s\n", strerror(errno));
					exit(1);
				}
			}
		}
		checkRequests(shmMsg, termMsg, pBlock, rBlock, clock, file);
	}

	sleep(5);
	fclose(file);
	clean(1);
	
	return 0;
}

void checkRequests(int *shmMsg, int *termMsg, pStruct *pBlock, rStruct *rBlock, int *clock, FILE* file){
	int pIndex = shmMsg[0];
	int rIndex = shmMsg[2];
	if(shmMsg[0] == -1 || shmMsg[2] == -1)
		return;
	int i, j;
	bool isDeadlock = false;
	//bool claimGranted = false;
	int killProcess = -1;
	bool hadResources = false;

	if(shmMsg[1] == 1){ //process is requesting claim
		if(verbose && lineCount < 100000){
			fprintf(file, "Master has detected P%d requesting R%d at time %d:%d\n", shmMsg[0], shmMsg[2], clock[0], clock[1]);
			lineCount++;
		}

		if(rBlock[rIndex].shared || rBlock[rIndex].numClaimed < rBlock[rIndex].num){ //grant claim request
			if(verbose && lineCount < 100000){
				fprintf(file, "Master granting P%d requesting R%d at time %d:%d\n", shmMsg[0], shmMsg[2], clock[0], clock[1]);
				lineCount++;
			}
			
			rBlock[rIndex].numClaimed++;
			grantedRequests++;
			grantedRequestsGlobal++;
			pBlock[pIndex].numClaimed++;
			pBlock[pIndex].resourceNum[rIndex]++;
			//claimGranted = true;
			if(!rBlock[rIndex].shared)
				allocatedMatrix[pIndex][rIndex]++;
			if(!rBlock[rIndex].shared && availableVector[rIndex] > 0)
				availableVector[rIndex]--;
			if(grantedRequests == 20){
				grantedRequests = 0;
				if(verbose && lineCount < 100000){
					fprintf(file, "Current system resources:\n");
					lineCount++;
					for(i = 0; i < 20; i++){
						fprintf(file, "\tR%d ", i);
						if(rBlock[i].shared)
							fprintf(file, "(s)");
					}
					fprintf(file, "\n");
					lineCount++;
					for(i = 0; i < 18; i++){
						fprintf(file, "P%d ", i);
						for(j = 0; j < 20; j++){
							fprintf(file, "\t%d", pBlock[i].resourceNum[j]);
						}
						fprintf(file, "\n");
						lineCount++;
					}
				}
			}
		}else if (!rBlock[rIndex].shared){ //deny claim request
			if(verbose && lineCount < 100000){
				fprintf(file, "Master denying P%d requesting R%d at time %d:%d\n", shmMsg[0], shmMsg[2], clock[0], clock[1]);
				lineCount++;
			}
			requestMatrix[pIndex][rIndex]++;
		}
		shmMsg[0] = -1;
		shmMsg[1] = -1;
		shmMsg[2] = -1;
		sb.sem_op = 1;
		sb.sem_num = 0;
		sb.sem_flg = 0;
		semop(sharedmem[2], &sb, 1);
	}else if(shmMsg[1] == 0){ //process is requesting release
		if(verbose && lineCount < 100000){
			fprintf(file, "Master has acknowledged P%d releasing R%d at time %d:%d\n", shmMsg[0], shmMsg[2], clock[0], clock[1]);
			lineCount++;
		}
//		if(pBlock[pIndex].resourceNum[rIndex] - 1 >= 0){
		if(!rBlock[rIndex].shared && allocatedMatrix[pIndex][rIndex] > 0)
			allocatedMatrix[pIndex][rIndex]--;
		rBlock[rIndex].numClaimed = rBlock[rIndex].numClaimed - pBlock[pIndex].resourceNum[rIndex];
		pBlock[pIndex].numClaimed = pBlock[pIndex].numClaimed - pBlock[pIndex].resourceNum[rIndex];
		pBlock[pIndex].resourceNum[rIndex] = 0;
//		}
		shmMsg[0] = -1;
		shmMsg[1] = -1;
		shmMsg[2] = -1;	
		sb.sem_op = 1;
		sb.sem_num = 0;
		sb.sem_flg = 0;
		semop(sharedmem[2], &sb, 1);
	}else if(shmMsg[1] == 2){ // process is terminating
		terminatedProcesses++;
		if(verbose && lineCount < 100000){
			fprintf(file, "Master has acknowledged P%d is terminating at time %d:%d\n", shmMsg[0], clock[0], clock[1]);
			lineCount++;
		}
	}
//	if(claimGranted){
//		for(i = 0; i < 20; i++)
//			availableVector[i] = rBlock[i].num - rBlock[i].numClaimed;
		/*
		printf("\tR0\tR1\tR2\tR3\tR4\tR5\tR6\tR7\tR8\tR9\tR10\tR11\tR12\tR13\tR14\tR15\tR16\tR17\tR18\tR19\n");
		for(i = 0; i < 18; i++){
			printf("P%d\t", i);
			for(j = 0; j < 20; j++){
				printf("%d\t", pBlock[i].resourceNum[j]);
			}
			printf("\n");
		}
		*/

		if(lineCount < 100000){
			fprintf(file, "Master running deadlock detection at time %d:%d\n", clock[0], clock[1]); //was causing seg fault
			lineCount++;
		}
		
		isDeadlock = true;

/*
//=================
		fprintf(file, "\nAVAILABLE VECTOR\n");
		for(i = 0; i < 20; i++)
			fprintf(file, "\tR%d ", i);
		fprintf(file, "\n");
		for(i = 0; i < 20; i++)
			fprintf(file, "\t%d", availableVector[i]);
		fprintf(file, "\nALLOCATED MATRIX\n");
		for(i = 0; i < 20; i++){
			fprintf(file, "\tR%d ", i);
		}
		fprintf(file, "\n");
		for(i = 0; i < 18; i++){
			fprintf(file, "P%d ", i);
			for(j = 0; j < 20; j++){
				fprintf(file, "\t%d", allocatedMatrix[i][j]);
			}
			fprintf(file, "\n");
			lineCount++;
		}
		fprintf(file, "\nREQUEST MATRIX\n");
		for(i = 0; i < 20; i++){
			fprintf(file, "\tR%d ", i);
		}
		fprintf(file, "\n");
		for(i = 0; i < 18; i++){
			fprintf(file, "P%d ", i);
			for(j = 0; j < 20; j++){
				fprintf(file, "\t%d", requestMatrix[i][j]);
			}
			fprintf(file, "\n");
			lineCount++;
		}
//===================
*/
//		while(isDeadlock){
			deadAlgRun++;
			isDeadlock = deadlock(availableVector, 20, 18, requestMatrix, allocatedMatrix, file, killProcess);
			killProcess = -1;
			if(isDeadlock)
				wasDeadlock = true;

			if(isDeadlock && lineCount < 100000){
				if(deadlockCount > 1){
					fprintf(file, "\tProcesses ");
					for(i = 0; i < 18; i++){
						if(deadlockedProcesses[i] == 1){
							fprintf(file, "P%d ", i);
							if(killProcess == -1)
								killProcess = i;
						}
					}
					fprintf(file, " deadlocked\n");
					lineCount++;
				}else if(deadlockCount == 1){
					fprintf(file, "\tProcess ");
					for(i = 0; i < 18; i++){
						if(deadlockedProcesses[i] == 1){
							fprintf(file, "P%d", i);
							if(killProcess == -1){
								killProcess = i;
							}
							break;
						}
					}
					fprintf(file, " deadlocked\n");
					lineCount++;
				}
	
				deadlockCount = 0;
				fprintf(file, "\tAttempting to resolve deadlock...\n");
				lineCount++;
				fprintf(file, "\tKilling process P%d\n", killProcess);
				lineCount++;
				fprintf(file, "\t\tResources released are as follows: ");
				if(killProcess != -1){
					for(i = 0; i < 20; i++){
						if(pBlock[killProcess].resourceNum[i] > 0){
							fprintf(file, "R%d:%d ", i, pBlock[killProcess].resourceNum[i]);
							hadResources = true;
						}
					}
					if(!hadResources)
						fprintf(file, "this process did not have any resources allocated to it");
					hadResources = false;
					fprintf(file, "\n");
					lineCount++;
				}
			}
			if(killProcess != -1){
				fprintf(file, "killProcess: %d\n", killProcess);
				for(i = 0; i < 18; i++)	
					deadlockedProcesses[i] = 0;
				killedProcesses++;
				kill(pBlock[killProcess].pid, SIGKILL);
				waitpid(pBlock[killProcess].pid, NULL, 0);
				pBlock[killProcess].numClaimed = 0;
				//pBlock[killProcess].pid = -1;
				clearProcess[killProcess] = 1;
				for(i = 0; i < 20; i++){
					if(pBlock[killProcess].resourceNum[i] > 0){
						rBlock[i].numClaimed = rBlock[i].numClaimed - pBlock[killProcess].resourceNum[i];
						availableVector[i] = availableVector[i] +  pBlock[killProcess].resourceNum[i];
						pBlock[killProcess].resourceNum[i] = 0;
						//requestMatrix[killProcess][i] = 0;
						fprintf(file, "allocatedMatrix[%d][%d]: %d\n", killProcess, i, allocatedMatrix[killProcess][i]);
						allocatedMatrix[killProcess][i] = 0;
						fprintf(file, "allocatedMatrix[%d][%d]: %d\n", killProcess, i, allocatedMatrix[killProcess][i]);
					}
					requestMatrix[killProcess][i] = 0;
				}
			}
/*
		//=================
                 fprintf(file, "\n\n\n\n\nAVAILABLE VECTOR==========================================\n");
                 for(i = 0; i < 20; i++)
                         fprintf(file, "\tR%d ", i);
                 fprintf(file, "\n");
                 for(i = 0; i < 20; i++)
                         fprintf(file, "\t%d", availableVector[i]);
                 fprintf(file, "\nALLOCATED MATRIX\n");
                 for(i = 0; i < 20; i++){
                         fprintf(file, "\tR%d ", i);
                 }
                 fprintf(file, "\n");
                 for(i = 0; i < 18; i++){
                         fprintf(file, "P%d ", i);
                                 for(j = 0; j < 20; j++){
                                         fprintf(file, "\t%d", allocatedMatrix[i][j]);
                                 }
                                 fprintf(file, "\n");
                                 lineCount++;
                 }
                 fprintf(file, "\nREQUEST MATRIX\n");
                 for(i = 0; i < 20; i++){
                         fprintf(file, "\tR%d ", i);
                 }
                 fprintf(file, "\n");
                 for(i = 0; i < 18; i++){
                         fprintf(file, "P%d ", i);
                        for(j = 0; j < 20; j++){
                                 fprintf(file, "\t%d", requestMatrix[i][j]);
                         }
                         fprintf(file, "\n");
                        lineCount++;
                 }
 //===================
*/
//		}
		killProcess = -1;
		if(wasDeadlock && lineCount < 100000){
			fprintf(file, "System is no longer in deadlock\n");
			lineCount++;
			wasDeadlock = false;
			deadlockFixed++;
		}else if(!wasDeadlock && lineCount < 100000){
			fprintf(file, "Master did not detect a deadlock\n");
			lineCount++;
		}else if(wasDeadlock){
			wasDeadlock = false;
		}
//		}

	return;
}

bool deadlock(const int *available, const int m, const int n, int request[18][20], int allocated[18][20], FILE *file, int killed){
	int work[m];
	bool finish[n];
	int i, p;
	bool isDeadlocked = false;

	if(killed != -1 && lineCount < 100000){
		fprintf(file, "\tMaster running deadlock detection after P%d killed\n", killed);
		lineCount++;
	}
	for (i = 0; i < m; i++)
		work[i] = available[i];
	for (i = 0; i < n; finish[i++] = false);
	for (p = 0; p < n; p++){
		if (finish[p])
			continue;
		if (req_lt_avail(request, work, p, m)){
			finish[p] = true;
			for (i = 0; i < m; i++)
				work[i] += allocated[p][m+i];
			p = -1;
		}
	}

	for (p = 0; p < n; p++){
		if (!finish[p]){
			deadlockedProcesses[p] = 1;
			deadlockCount++;
			isDeadlocked = true;
		}
	}
	return isDeadlocked; 
}

bool req_lt_avail(int req[18][20], const int *avail, const int pnum, const int num_res){
	int i;
	for (i = 0; i < num_res; i++)
		if (req[pnum][num_res+i] > avail[i])
			break;
	return (i == num_res);
}
