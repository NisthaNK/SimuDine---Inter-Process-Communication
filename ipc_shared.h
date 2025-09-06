#ifndef IPC_SHARED_H
#define IPC_SHARED_H

#include <sys/types.h>
#include <sys/ipc.h>
#include <sys/shm.h>
#include <sys/sem.h>
#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>
#include <sys/wait.h>

#define MAX_CUSTOMERS 200
#define MAX_TABLES 10
#define NUM_WAITERS 5
#define PROJ_ID 42
#define SHM_SIZE 2000
#define QUEUE_SIZE 100

// Shared memory structure starts with the first 100 cells
// M[0] = time (initialized to 0)
// M[1] = empty tables (initialized to 10)
// M[2] = next waiter to serve (initialized to 0)
// M[3] = pending orders for cooks (initialized to 0)

// Semaphore indices
enum {
    MUTEX_SEM = 0,     // Protects shared memory
    COOK_SEM,          // Signals cooks
    WAITER_SEM_BASE,   // Base index for waiter semaphores
    CUSTOMER_SEM_BASE = WAITER_SEM_BASE + NUM_WAITERS // Base index for customer semaphores
};

// Total semaphores needed
#define TOTAL_SEMS (CUSTOMER_SEM_BASE + MAX_CUSTOMERS)

// Convenience macro for accessing shared memory
#define TIME(shm) shm[0]
#define EMPTY_TABLES(shm) shm[1]
#define NEXT_WAITER(shm) shm[2]
#define PENDING_ORDERS(shm) shm[3]

// Waiter area offsets
#define WAITER_AREA_SIZE 200
#define WAITER_AREA_START 100
#define WAITER_AREA(w) (WAITER_AREA_START + (w) * WAITER_AREA_SIZE)

// Waiter queue index offsets
#define WAITER_FRONT(w) (WAITER_AREA(w))
#define WAITER_REAR(w) (WAITER_AREA(w) + 1)
#define WAITER_FOOD_READY(w) (WAITER_AREA(w) + 2)
#define WAITER_PENDING_ORDERS(w) (WAITER_AREA(w) + 3)
#define WAITER_QUEUE_START(w) (WAITER_AREA(w) + 4)

// Cook queue offsets
#define COOK_QUEUE_START 1100
#define COOK_FRONT (COOK_QUEUE_START)
#define COOK_REAR (COOK_QUEUE_START + 1)
#define COOK_QUEUE_DATA (COOK_QUEUE_START + 2)

// For semctl initialization
union semun {
    int val;
    struct semid_ds *buf;
    unsigned short *array;
};

// Utility functions for semaphores
static void take(int semid, int sem_num) {
    struct sembuf sb;
    sb.sem_num = sem_num;
    sb.sem_op = -1;
    sb.sem_flg = 0;
    if (semop(semid, &sb, 1) == -1) {
        perror("semop: take");
        exit(1);
    }
}

static void put(int semid, int sem_num) {
    struct sembuf sb;
    sb.sem_num = sem_num;
    sb.sem_op = 1;
    sb.sem_flg = 0;
    if (semop(semid, &sb, 1) == -1) {
        perror("semop: put");
        exit(1);
    }
}

// Common function to get IPC keys
static key_t get_key() {
    key_t key = ftok("/tmp", PROJ_ID);
    if (key == -1) {
        perror("ftok");
        exit(1);
    }
    return key;
}

static void add_cooking_request(int *shm, int waiter_id, int customer_id, int count) {
    int rear = shm[COOK_REAR];
    shm[COOK_QUEUE_DATA + rear*3] = waiter_id;
    shm[COOK_QUEUE_DATA + rear*3 + 1] = customer_id;
    shm[COOK_QUEUE_DATA + rear*3 + 2] = count;
    shm[COOK_REAR] = (rear + 1) % QUEUE_SIZE;
    PENDING_ORDERS(shm)++;
}

static void get_cooking_request(int *shm, int *waiter_id, int *customer_id, int *count) {
    int front = shm[COOK_FRONT];
    *waiter_id = shm[COOK_QUEUE_DATA + front*3];
    *customer_id = shm[COOK_QUEUE_DATA + front*3 + 1];
    *count = shm[COOK_QUEUE_DATA + front*3 + 2];
    shm[COOK_FRONT] = (front + 1) % QUEUE_SIZE;
    PENDING_ORDERS(shm)--;
}

// Update simulated time
static void update_time(int *shm, int minutes) {
    int curr_time = TIME(shm);
    usleep(minutes * 100000);  // Scale: 1 minute = 100ms
    
    // Only update if new time is greater
    if (curr_time + minutes > TIME(shm)) {
        TIME(shm) = curr_time + minutes;
    }
}

#endif // IPC_SHARED_H