#include "ipc_shared.h"
#include <signal.h>
#include <time.h>

// Global IPC identifiers for cleanup
int shmid = -1;
int semid = -1;

// Signal handler for graceful termination
void cleanup_handler(int sig) {
    printf("Cook process received signal %d, cleaning up...\n", sig);
    if (shmid != -1) shmctl(shmid, IPC_RMID, NULL);
    if (semid != -1) semctl(semid, 0, IPC_RMID);
    exit(1);
}

// Function to create and initialize shared memory
int create_shared_memory() {
    key_t key = get_key();
    int id = shmget(key, SHM_SIZE * sizeof(int), IPC_CREAT | 0666);
    if (id == -1) {
        perror("shmget");
        exit(1);
    }
    
    // Attach to shared memory and initialize
    int *shm = (int *)shmat(id, NULL, 0);
    if (shm == (void *) -1) {
        perror("shmat");
        exit(1);
    }
    
    // Initialize shared memory
    TIME(shm) = 0;                 // Current time (minutes after 11:00am)
    EMPTY_TABLES(shm) = MAX_TABLES; // Initially all tables are empty
    NEXT_WAITER(shm) = 0;          // Next waiter to serve
    PENDING_ORDERS(shm) = 0;       // No pending orders initially
    
    // Initialize waiter queues
    for (int i = 0; i < NUM_WAITERS; i++) {
        shm[WAITER_FRONT(i)] = 0;
        shm[WAITER_REAR(i)] = 0;
        shm[WAITER_FOOD_READY(i)] = -1;       // No food ready
        shm[WAITER_PENDING_ORDERS(i)] = 0;    // No pending orders
    }
    
    // Initialize cook queue
    shm[COOK_FRONT] = 0;
    shm[COOK_REAR] = 0;
    
    printf("Shared memory initialized\n");
    
    // Detach from shared memory
    shmdt(shm);
    return id;
}

// Function to create and initialize semaphores
int create_semaphores() {
    key_t key = get_key();
    int id = semget(key, TOTAL_SEMS, IPC_CREAT | 0666);
    if (id == -1) {
        perror("semget");
        exit(1);
    }
    
    // Initialize semaphores
    union semun arg;
    unsigned short values[TOTAL_SEMS];
    
    // Initialize mutex to 1
    values[MUTEX_SEM] = 1;
    
    // Initialize cook semaphore to 0
    values[COOK_SEM] = 0;
    
    // Initialize waiter semaphores to 0
    for (int i = 0; i < NUM_WAITERS; i++) {
        values[WAITER_SEM_BASE + i] = 0;
    }
    
    // Initialize customer semaphores to 0
    for (int i = 0; i < MAX_CUSTOMERS; i++) {
        values[CUSTOMER_SEM_BASE + i] = 0;
    }
    
    arg.array = values;
    if (semctl(id, 0, SETALL, arg) == -1) {
        perror("semctl");
        exit(1);
    }
    
    printf("Semaphores initialized\n");
    return id;
}

// Function executed by each cook process
void cmain(int cook_id, int shmid, int semid) {
    printf("Cook %c started (PID: %d)\n", 'C' + cook_id, getpid());
    
    // Attach to shared memory
    int *shm = (int *)shmat(shmid, NULL, 0);
    if (shm == (void *) -1) {
        perror("shmat in cook");
        exit(1);
    }
    
    int is_last_cook = 0;
    
    while (1) {
        // Wait for a cooking request
        take(semid, COOK_SEM);
        
        // Check if it's time to end the session
        take(semid, MUTEX_SEM);
        if (TIME(shm) >= 180 && PENDING_ORDERS(shm) == 0) {
            // Time is past 3:00pm and no more orders
            is_last_cook = 1;
            put(semid, MUTEX_SEM);
            break;
        }
        
        // Process cooking request
        if (shm[COOK_FRONT] != shm[COOK_REAR]) {
            int waiter_id, customer_id, count;
            get_cooking_request(shm, &waiter_id, &customer_id, &count);
            
            printf("Cook %c preparing food for customer %d (party size: %d, waiter: %c)\n", 
                   'C' + cook_id, customer_id, count, 'U' + waiter_id);
            
            put(semid, MUTEX_SEM);
            
            // Simulate cooking time (5 minutes per person)
            update_time(shm, count * 5);
            
            take(semid, MUTEX_SEM);
            
            // Notify waiter that food is ready
            shm[WAITER_FOOD_READY(waiter_id)] = customer_id;
            printf("Cook %c finished preparing food for customer %d\n", 'C' + cook_id, customer_id);
            
            // Signal the waiter
            put(semid, MUTEX_SEM);
            put(semid, WAITER_SEM_BASE + waiter_id);
        } else {
            put(semid, MUTEX_SEM);
        }
    }
    
    // If this is the last cook, wake all waiters
    if (is_last_cook) {
        printf("Cook %c is the last cook, waking all waiters to end session\n", 'C' + cook_id);
        for (int i = 0; i < NUM_WAITERS; i++) {
            put(semid, WAITER_SEM_BASE + i);
        }
    }
    
    // Detach from shared memory
    shmdt(shm);
    printf("Cook %c terminated\n", 'C' + cook_id);
    exit(0);
}

int main() {
    // Set up signal handlers
    signal(SIGINT, cleanup_handler);
    signal(SIGTERM, cleanup_handler);
    
    printf("Restaurant simulation starting...\n");
    
    // Create and initialize IPC resources
    shmid = create_shared_memory();
    semid = create_semaphores();
    
    // Fork cook processes
    pid_t pid1 = fork();
    if (pid1 == 0) {
        // Cook C
        cmain(0, shmid, semid);
        exit(0);
    }
    
    pid_t pid2 = fork();
    if (pid2 == 0) {
        // Cook D
        cmain(1, shmid, semid);
        exit(0);
    }
    
    // Wait for cook processes to finish
    printf("Waiting for cooks to finish...\n");
    wait(NULL);
    wait(NULL);
    
    printf("All cooks have finished. Exiting cook parent process.\n");
    exit(0);
    
    return 0;
}