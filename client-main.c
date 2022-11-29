//
//  main.c
//  shm-client
//
//  Created by Dante Mattson on 24/9/20.
//

#include <stdio.h>
#include <unistd.h>
#include <stdlib.h>
#include <string.h>
#include <time.h>
#include <stdint.h>
#include <sys/types.h>
#include <sys/ipc.h>
#include <sys/shm.h>
#include <semaphore.h>
#include <stdbool.h>

#include "common.h"

// Checks if a string contains any invalid characters
bool isDigit(char* str) {
    
    for (int i = 0; i < strlen(str); i++) {
        
        if (!(str[i] == '0' || str[i] == '1' || str[i] == '2' || str[i] == '3' || str[i] == '4' || str[i] == '5' || str[i] == '6' || str[i] == '7' || str[i] == '8' || str[i] == '9' || str[i] == '\n')) {
            return false;
        }
        
    }
    
    return true;
}

// main
int main(int argc, const char * argv[]) {
    
    int clientFlagSharedId;
    int serverFlagSharedId;
    int currentResultId;
    int progressId;
    
    sem_t *numbersem;
    sem_t *progresssem;
    sem_t *sflagsem;
    sem_t *cflagsem;

    int *progress;
    
    // Keep track of time of requests
    clock_t *starts = (clock_t *)calloc(MAX_QUERIES, sizeof(clock_t));
    
    // shared
    char *clientFlag;
    // not an array
    char *serverFlag;
    int *sharedQuit;
    
    int sharedQuitId;
    unsigned int *currentResult;
    
    // Connect to shared memory
    clientFlagSharedId = shmget(SHARED_CLIENT_FLAG_KEY, sizeof(char), IPC_EXCL | 0777);
    serverFlagSharedId = shmget(SHARED_SERVER_FLAG_KEY, sizeof(*serverFlag), IPC_EXCL | 0777);
    currentResultId = shmget(CURRENTRESULT_KEY, sizeof(unsigned int), IPC_EXCL | 0777);
    progressId = shmget(PROGRESS_KEY, sizeof(int)*MAX_QUERIES, IPC_EXCL | 0777);
    sharedQuitId = shmget(SHARED_QUIT_KEY, sizeof(int), IPC_EXCL | 0777);
    
    // Error checking / handling
    if (clientFlagSharedId < 0) {
        printf("Unable to create client shared memory\n");
        return 1;
    }
    
    if (serverFlagSharedId < 0) {
        printf("Unable to create server shared memory\n");
        return 1;
    }
    
    if (currentResultId < 0) {
        printf("Unable to create shared number memory\n");
        return 1;
    }
    
    if (progressId < 0) {
        printf("Unable to create progress memory\n");
        return 1;
    }
    
    if (sharedQuitId < 0) {
        printf("Unable to create sharedQuit memory\n");
        return 1;
    }
    
    // Attach shared memory
    clientFlag = (char *)shmat(clientFlagSharedId, NULL, 0);
    serverFlag = (char *)shmat(serverFlagSharedId, NULL, 0);
    currentResult = (unsigned int*)shmat(currentResultId, NULL, 0);
    progress = (int *)shmat(progressId, NULL, 0);
    sharedQuit = (int *)shmat(sharedQuitId, NULL, 0);
    
    // Attach semaphores
    numbersem = sem_open("/number", O_EXCL);
    sflagsem = sem_open("/serverflag", O_EXCL);
    cflagsem = sem_open("/clientflag", O_EXCL);
    progresssem = sem_open("/progresssem", O_EXCL);
    
    int inputForkId = -1;
    
    if ((inputForkId = fork()) == 0) {
        
        char inputString[BUFLEN];
        int didFindSlot = 0;
        int didGetDigit = 0;
        
        // Input loop
        printf("Input: ");
        while (fgets(inputString, BUFLEN, stdin) != NULL && *sharedQuit == 0) {
            
            if (inputString[0] == 'q') {
                *sharedQuit = 1;
                exit(0);
            }
            
            if (inputString[0] != '\n' && isDigit(inputString)) {
                
                didGetDigit = 1;
                
                int rawInt = atoi(inputString);
                unsigned int inputInt = (unsigned int)rawInt;
                
                printf("read in %u\n", inputInt);
                
                // allocate slot on server
                printf("searching for slot\n");
                
                for (int i = 0; i < MAX_QUERIES; i++) {
                    
                    if (clientFlag[i] == 'w') {
                        printf("found open slot\n");
                        
                        starts[i] = clock();
                        
                        didFindSlot = 1;
                        // send request
                        sem_wait(progresssem);
                        progress[i] = 0;
                        sem_post(progresssem);
                        
                        //printf("waiting for cflagsem\n");
                        sem_wait(cflagsem);
                        clientFlag[i] = 'e';
                        sem_post(cflagsem);
                        
                        while (clientFlag[i] == 'e') {}
                        printf("handshake\n");
                        
                        //printf("waiting for numbersem\n");
                        sem_wait(numbersem);
                        *currentResult = inputInt;
                        sem_post(numbersem);
                        
                        // client wrote
                        sem_wait(cflagsem);
                        clientFlag[i] = CLIENT_WROTE;
                        sem_post(cflagsem);
                        
                        break;
                    }
                    
                }
            
            }
            
            if (didGetDigit == 0) {
                printf("Not a digit!\n");
            }
            
            if (didFindSlot == 0) {
                printf("Server busy\n");
            }
            
            didFindSlot = 0;
            didGetDigit = 0;
            
            printf("\n");
            printf("Input: ");
            
        
        }
        
        exit(0);
    } else if (inputForkId < 0) {
        printf("Unable to fork to send request");
        exit(0);
    }
    
    
    struct timespec start, end;
    clock_gettime(CLOCK_MONOTONIC_RAW, &start);
    time_t diff = 0;
    
    while (1 && *sharedQuit == 0) {
        
        if (*serverFlag == '1') {
            printf("client read number: %d\n", *currentResult);
            
            sem_wait(sflagsem);
            *serverFlag = '0';
            sem_post(sflagsem);
            
        }
        
        // Update elapsed time
        clock_gettime(CLOCK_MONOTONIC_RAW, &end);
        
        if ( (((end.tv_sec - start.tv_sec) * 1000000.0 + (end.tv_nsec - start.tv_nsec) / 1000.0)) >= 500000.0) {
            
            for (int i = 0; i < MAX_QUERIES; i++) {
                
                if (progress[i] != 0) {
                    sem_wait(progresssem);
                    printf("Query %d progress: %d/32\n", i, progress[i]);
                    sem_post(progresssem);
                }
                
                if (clientFlag[i] == 'c') {
                    
                    clock_t current = clock();
                    printf("Request %d is done. Time taken: %f\n", i, ((current - starts[i] - diff) / (float)CLOCKS_PER_SEC));
                    diff = current - starts[i];
                    starts[i] = NULL;
                    
                    sem_wait(cflagsem);
                    clientFlag[i] = 'd';
                    sem_post(cflagsem);
                    
                }
                
            }
            
            //reset timer
            clock_gettime(CLOCK_MONOTONIC_RAW, &start);
        }
        
        
    }
    
    shmdt(clientFlag);
    shmdt(serverFlag);
    shmdt(currentResult);
    shmdt(progress);
    
    return 0;
}
