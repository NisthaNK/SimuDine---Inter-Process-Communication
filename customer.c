#include "ipc_shared.h"
#include <time.h>

// Function executed by each customer process
void cmain(int customer_id, int arrival_time, int party_size, int shmid, int semid) {
    // Attach to shared memory
    int *shm = (int *)shmat(shmid, NULL, 0);
    if (shm == (void *) -1) {
        perror("shmat in customer");
        exit(1);
    }
    
    printf("Customer %d (party size: %d) arrived at %d minutes after 11:00am\n", 
           customer_id, party_size, arrival_time);
    
    // Set arrival time if it's greater than current time
    take(semid, MUTEX_SEM);
    if (arrival_time > TIME(shm)) {
        TIME(shm) = arrival_time;
    }
    
    // Check if restaurant is still open
    if (TIME(shm) >= 180) { // 3:00pm = 180 minutes after 11:00am
        printf("Customer %d arrived after closing time and left\n", customer_id);
        put(semid, MUTEX_SEM);
        shmdt(shm);
        exit(0);
    }
    
    // Check if table is available
    if (EMPTY_TABLES(shm) <= 0) {
        printf("Customer %d couldn't find an empty table and left\n", customer_id);
        put(semid, MUTEX_SEM);
        shmdt(shm);
        exit(0);
    }
    
    // Occupy a table
    EMPTY_TABLES(shm)--;
    printf("Customer %d occupied a table (%d tables remaining)\n", 
           customer_id, EMPTY_TABLES(shm));
    
    // Get assigned waiter
    int waiter_id = NEXT_WAITER(shm);
    NEXT_WAITER(shm) = (waiter_id + 1) % NUM_WAITERS;
    
    // Add to waiter's queue
    int rear = shm[WAITER_REAR(waiter_id)];
    shm[WAITER_QUEUE_START(waiter_id) + rear*2] = customer_id;
    shm[WAITER_QUEUE_START(waiter_id) + rear*2 + 1] = party_size;
    shm[WAITER_REAR(waiter_id)] = (rear + 1) % QUEUE_SIZE;
    shm[WAITER_PENDING_ORDERS(waiter_id)]++;
    
    printf("Customer %d is assigned to waiter %c\n", customer_id, 'U' + waiter_id);
    
    // Signal waiter
    put(semid, WAITER_SEM_BASE + waiter_id);
    put(semid, MUTEX_SEM);
    
    // Wait for food to be served
    take(semid, CUSTOMER_SEM_BASE + customer_id);
    
    printf("Customer %d received food and is eating\n", customer_id);
    
    // Eat food (30 minutes)
    update_time(shm, 30);
    
    // Free the table
    take(semid, MUTEX_SEM);
    EMPTY_TABLES(shm)++;
    printf("Customer %d finished eating and left (%d tables now available)\n", 
           customer_id, EMPTY_TABLES(shm));
    put(semid, MUTEX_SEM);
    
    // Detach from shared memory
    shmdt(shm);
    exit(0);
}

int main() {
    FILE *fp;
    int customer_id, arrival_time, party_size;
    int prev_arrival_time = 0;
    
    printf("Customer processes starting...\n");
    
    // Get the existing IPC resources
    key_t key = get_key();
    int shmid = shmget(key, 0, 0666);
    if (shmid == -1) {
        perror("shmget in customer");
        exit(1);
    }
    
    int semid = semget(key, TOTAL_SEMS, 0666);
    if (semid == -1) {
        perror("semget in customer");
        exit(1);
    }
    
    // Open customer input file
    fp = fopen("customers.txt", "r");
    if (fp == NULL) {
        perror("Error opening customers.txt");
        exit(1);
    }
    
    // Array to store child PIDs
    pid_t *child_pids = malloc(MAX_CUSTOMERS * sizeof(pid_t));
    if (child_pids == NULL) {
        perror("malloc");
        exit(1);
    }
    
    int customer_count = 0;
    
    // Read customer data and create processes
    while (fscanf(fp, "%d %d %d", &customer_id, &arrival_time, &party_size) == 3) {
        if (customer_id == -1) break;
        if(customer_id < 0 || arrival_time < 0 || party_size < 0) {
            exit(1);
        }

        
        // Wait for the time difference between consecutive customers
        if (arrival_time > prev_arrival_time) {
            usleep((arrival_time - prev_arrival_time) * 100000); // Scale: 1 minute = 100ms
        }
        prev_arrival_time = arrival_time;
        
        // Fork a child process for the customer
        child_pids[customer_count] = fork();
        
        if (child_pids[customer_count] == 0) {
            // Child process - customer
            cmain(customer_id, arrival_time, party_size, shmid, semid);
            // Should not reach here as cmain calls exit()
            exit(0);
        }
        
        customer_count++;
    }
    
    fclose(fp);
    
    // Wait for all customer processes to finish
    for (int i = 0; i < customer_count; i++) {
        waitpid(child_pids[i], NULL, 0);
    }
    
    printf("All customers have finished. Cleaning up IPC resources.\n");
    
    // Clean up IPC resources
    if (shmctl(shmid, IPC_RMID, NULL) == -1) {
        perror("shmctl");
    }
    
    if (semctl(semid, 0, IPC_RMID) == -1) {
        perror("semctl");
    }
    
    free(child_pids);
    printf("Restaurant simulation completed.\n");
    
    return 0;
}