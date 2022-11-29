//
//  common.h
//
//  Created by Dante Mattson on 25/9/20.
//

#include <stdint.h>
#include <time.h>

#ifndef common_h
#define common_h

#define MAX_QUERIES 10
#define NUM_THREADS ((INT_SIZE * 8) * MAX_QUERIES)

#define SHARED_CLIENT_FLAG_KEY 2020
#define SHARED_SERVER_FLAG_KEY 2021
#define CURRENTRESULT_KEY 2022
#define PROGRESS_KEY 2023
#define SHARED_QUIT_KEY 2024
#define SHMSIZE 32

#define CLIENT_WRITE '0'
#define CLIENT_WROTE '1'
#define SERVER_READ  '1'
#define SERVER_WROTE '0'

pthread_t threads[320];

#define BUFLEN 512

extern inline /* msleep(): Sleep for the requested number of milliseconds. */
void msleep(long msec)
{
    struct timespec ts;

    ts.tv_sec = msec / 1000;
    ts.tv_nsec = (msec % 1000) * 1000000;

    nanosleep(&ts, &ts);
}

#endif /* common_h */
