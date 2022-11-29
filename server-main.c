//
//  main.c
//  shm-server
//
//  Created by Dante Mattson on 24/9/20.
//

#include <stdio.h>
#include <stdlib.h>
#include <sys/types.h>
#include <unistd.h>
#include <time.h>
#include <sys/shm.h>
#include <semaphore.h>
#include <pthread/pthread.h>

#include "common.h"

// Hold references to semaphores and shared memory for access in threads
struct workData {
    int input;
    int queryNumber;
    char *serverFlag;
    char *clientFlag; // array
    int threadNum;
    int *progress;
    unsigned int *sharedNumber;
    sem_t *numbersem;
    sem_t *sflagsem;
    sem_t *progresssem;
};

struct joinerData {
    pthread_t *threads;
    int *progress;
    sem_t *progresssem;
    sem_t *cflagsem;
    char *clientFlag;
    int queryNumber;
};

// Join threads back to main using another thread
void threadJoiner(struct joinerData* wd) {
    
    for (int j = 0; j < 32; j++) {
        pthread_join(threads[(wd->queryNumber * 32) + j], NULL);
    }
    
    pthread_exit(NULL);
    
}

// Perform factorisation and send result to client
void threadWorker(struct workData* wd) {
    
    for (int i = 2; i < wd->input; i++) {
        if (wd->input % i == 0) {
            // your turn to write
            sem_wait(wd->numbersem);
            *(wd->sharedNumber) = wd->input;
            sem_wait(wd->sflagsem);
            *(wd->serverFlag) = '1';
            sem_post(wd->sflagsem);
            sem_post(wd->numbersem);
            
        }
    }
    
    sem_wait(wd->progresssem);
    wd->progress[wd->queryNumber] += 1;
    sem_post(wd->progresssem);
    
    pthread_exit(NULL);
}

// Shared quit flag
int *sharedQuit;

// main
int main(int argc, const char * argv[]) {
    
    // Semaphores
    sem_t *numbersem;

    sem_t *sflagsem;
    sem_t *cflagsem;
    
    sem_t *progresssem;
    
    // ints
    int *progress;
    unsigned int *currentResult;
    
    // Check to make sure unsigned int is 32 bit as per task
    if (sizeof(unsigned int) * 8 != 32) {
        printf("Unsigned int is not 32 bit on your system, which fails the requirements for this task.\n");
        exit(-1);
    }
    
    // Create semaphores
    printf("creating semaphores\n");
    numbersem = sem_open("/number", O_CREAT, 0755, 0);
    sem_post(numbersem);
    
    sflagsem = sem_open("/serverflag", O_CREAT, 0755, 0);
    sem_post(sflagsem);
    
    cflagsem = sem_open("/clientflag", O_CREAT, 0755, 0);
    sem_post(cflagsem);
    
    progresssem = sem_open("/progresssem", O_CREAT, 0755, 0);
    sem_post(progresssem);
    
    // array
    char *clientFlag;
    // shared
    char *serverFlag;
    
    int clientFlagSharedId;
    int serverFlagSharedId;
    int currentResultId;
    int progressId;
    int sharedQuitId;
    
    // Create shared memory
    clientFlagSharedId = shmget(SHARED_CLIENT_FLAG_KEY, sizeof(char), IPC_CREAT | 0777);
    serverFlagSharedId = shmget(SHARED_SERVER_FLAG_KEY, sizeof(*serverFlag)*10, IPC_CREAT | 0777);
    currentResultId = shmget(CURRENTRESULT_KEY, sizeof(unsigned int), IPC_CREAT | 0777);
    progressId = shmget(PROGRESS_KEY, sizeof(int)*10, IPC_CREAT | 0777);
    sharedQuitId = shmget(SHARED_QUIT_KEY, sizeof(int), IPC_CREAT | 0777);
    
    // Check all OK
    if (clientFlagSharedId < 0) {
        perror("Unable to create client shared memory\n");
        return 1;
    }
    
    if (serverFlagSharedId < 0) {
        printf("Unable to create server shared memory\n");
        return 1;
    }
    
    if (currentResultId < 0) {
        printf("Unable to create current result memory\n");
        return 1;
    }
    
    if (progressId < 0) {
        printf("Unable to create progress memory\n");
        return 1;
    }
    
    if (sharedQuitId < 0) {
        printf("Unable to create sharedQuit shared memory\n");
        return 1;
    }
    
    // Attach shared memory
    clientFlag = (char *)shmat(clientFlagSharedId, NULL, 0);
    serverFlag = (char *)shmat(serverFlagSharedId, NULL, 0);
    currentResult = (unsigned int*)shmat(currentResultId, NULL, 0);
    progress = (int *)shmat(progressId, NULL, 0);
    sharedQuit = (int *)shmat(sharedQuitId, NULL, 0);
    
    // Check creation was successful
    if (clientFlag == (char*) -1 || serverFlag == (char*) -1) {
        printf("Unable to connect to shared memory\n");
        return 1;
    }
    
    // Open all slots for client
    for (int i = 0; i < MAX_QUERIES; i++) {
        progress[i] = 0;
        clientFlag[i] = 'w';
    }
    
    // Default values / first run
    *currentResult = 0;
    *sharedQuit = 0;
    
    printf("created shared memory\n");
    
    printf("waiting for clients\n");
    int threadResult;
    unsigned int input;
    
    // While run and should not quit
    while (1 && *sharedQuit == 0) {
        
        // For every slot
        for (int i = 0; i < MAX_QUERIES; i++) {
            
            // Client attempting handshake
            if (clientFlag[i] == 'e') {
                printf("accepted client\n");
                
                // allow client write
                sem_wait(cflagsem);
                clientFlag[i] = '0';
                sem_post(cflagsem);
                
                // wait
                printf("wait for handshake\n");
                while (*clientFlag == '0') {}
                printf("handshake\n");
                
                // read
                sem_wait(numbersem);
                input = *currentResult;
                sem_post(numbersem);
                
                // request / slot num is 'i' here
                for (int j = 0; j < 32; j++) {
                    
                    int threadNumber = (i * 32) + j;
                    
                    // threads and their data
                    struct workData *wd;
                    wd = malloc(sizeof(struct workData));
                    
                    wd->clientFlag = clientFlag;
                    wd->progress = progress;
                    wd->queryNumber = i;
                    wd->serverFlag = serverFlag;
                    wd->threadNum = threadNumber;
                    wd->sharedNumber = currentResult;
                    wd->sflagsem = sflagsem;
                    wd->numbersem = numbersem;
                    wd->progresssem = progresssem;
                    
                    // First result will be the number from the client
                    if (j == 0) {
                        wd->input = input;
                    } else {
                        // Anything else will be a thread started with right binary rotation of j
                        wd->input = (j >> input) | (j << (32 - input));
                    }
                    
                    // Create the thread
                    threadResult = pthread_create(&threads[threadNumber], NULL, threadWorker, (void*)wd);
                    
                    // Check it was successful
                    if (threadResult != 0) {
                        printf("Error creating thread/s: threadResult %d\n", threadResult);
                    }
                    
                }
                
                // Indicate the client should read
                sem_wait(cflagsem);
                *clientFlag = SERVER_WROTE;
                sem_post(cflagsem);
                printf("started threads for client\n");
                
                // Join
                struct joinerData *jd;
                jd = malloc(sizeof(struct joinerData));
                
                jd->threads = threads;
                jd->cflagsem = cflagsem;
                jd->clientFlag = clientFlag;
                jd->progress = progress;
                jd->progresssem = progresssem;
                jd->queryNumber = i;
                
                pthread_t jointhreadId;
                
                pthread_create(&jointhreadId, NULL, threadJoiner, (void*)jd);
                pthread_join(jointhreadId, NULL);
                
                free(jd);
                
                // Reset
                sem_wait(progresssem);
                progress[i] = 0;
                sem_post(progresssem);
                
                // Tells client to calculate the time taken for the request
                sem_wait(cflagsem);
                clientFlag[i] = 'c';
                sem_post(cflagsem);
                
            }
            
            // Client has calculated time and that slot can be reused
            if (clientFlag[i] == 'd') {
                sem_wait(cflagsem);
                clientFlag[i] = 'w';
                sem_post(cflagsem);
            }
            
        }
        
    }
    
    // Dispose of semaphores and shared memory once quit is requested
    sem_unlink ("/number");
    sem_unlink ("/serverflag");
    sem_unlink ("/clientflag");
    sem_unlink ("/progresssem");
    
    shmdt(clientFlag);
    shmdt(serverFlag);
    shmdt(currentResult);
    shmdt(progress);
    shmdt(sharedQuit);
    
    return 0;
}
