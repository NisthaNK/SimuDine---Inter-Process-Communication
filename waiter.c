#include "ipc_shared.h"
#include <signal.h>
#include <time.h>

// Global IPC identifiers for cleanup
int shmid = -1;
int semid = -1;

// Signal handler for graceful termination
void cleanup_handler(int sig) {
    printf("Waiter process received signal %d, cleaning up...\n", sig);
    if (shmid != -1) shmctl(shmid, IPC_RMID, NULL);
    if (semid != -1) semctl(semid, 0, IPC_RMID);
    exit(1);
}

// Function executed by each waiter process
void wmain(int waiter_id, int shmid, int semid) {
    char waiter_name = 'U' + waiter_id;
    printf("Waiter %c started (PID: %d)\n", waiter_name, getpid());
    
    // Attach to shared memory
    int *shm = (int *)shmat(shmid, NULL, 0);
    if (shm == (void *) -1) {
        perror("shmat in waiter");
        exit(1);
    }
    
    while (1) {
        // Wait for signal (from cook or customer)
        take(semid, WAITER_SEM_BASE + waiter_id);
        
        // Check if session should end
        take(semid, MUTEX_SEM);
        if (TIME(shm) >= 180 && shm[WAITER_PENDING_ORDERS(waiter_id)] == 0) {
            put(semid, MUTEX_SEM);
            break;
        }
        
        // Check if food is ready to be served
        if (shm[WAITER_FOOD_READY(waiter_id)] != -1) {
            int customer_id = shm[WAITER_FOOD_READY(waiter_id)];
            printf("Waiter %c serving food to customer %d\n", waiter_name, customer_id);
            
            // Reset food ready flag
            shm[WAITER_FOOD_READY(waiter_id)] = -1;
            
            // Signal customer that food is ready
            put(semid, MUTEX_SEM);
            put(semid, CUSTOMER_SEM_BASE + customer_id);
        }
        // Check if there's a new customer order to process
        else if (shm[WAITER_PENDING_ORDERS(waiter_id)] > 0) {
            int front = shm[WAITER_FRONT(waiter_id)];
            int customer_id = shm[WAITER_QUEUE_START(waiter_id) + front*2];
            int count = shm[WAITER_QUEUE_START(waiter_id) + front*2 + 1];
            
            // Move front of queue
            shm[WAITER_FRONT(waiter_id)] = (front + 1) % QUEUE_SIZE;
            shm[WAITER_PENDING_ORDERS(waiter_id)]--;
            
            printf("Waiter %c taking order from customer %d (party size: %d)\n", 
                   waiter_name, customer_id, count);
                   
            put(semid, MUTEX_SEM);
            
            // Simulate time to take order (1 minute)
            update_time(shm, 1);
            
            // Add order to cook queue
            take(semid, MUTEX_SEM);
            add_cooking_request(shm, waiter_id, customer_id, count);
            printf("Waiter %c submitted order for customer %d to kitchen\n", waiter_name, customer_id);
            
            // Signal cook that new order is available
            put(semid, MUTEX_SEM);
            put(semid, COOK_SEM);
        } else {
            put(semid, MUTEX_SEM);
        }
    }
    
    // Detach from shared memory
    shmdt(shm);
    printf("Waiter %c terminated\n", waiter_name);
    exit(0);
}

int main() {
    // Set up signal handlers
    signal(SIGINT, cleanup_handler);
    signal(SIGTERM, cleanup_handler);
    
    printf("Waiter processes starting...\n");
    
    // Get the existing IPC resources
    key_t key = get_key();
    shmid = shmget(key, 0, 0666);
    if (shmid == -1) {
        perror("shmget in waiter");
        exit(1);
    }
    
    semid = semget(key, TOTAL_SEMS, 0666);
    if (semid == -1) {
        perror("semget in waiter");
        exit(1);
    }
    
    // Fork waiter processes
    pid_t waiter_pids[NUM_WAITERS];
    
    for (int i = 0; i < NUM_WAITERS; i++) {
        waiter_pids[i] = fork();
        if (waiter_pids[i] == 0) {
            // Child process - waiter
            wmain(i, shmid, semid);
            exit(0);
        }
    }
    
    // Wait for all waiter processes to finish
    for (int i = 0; i < NUM_WAITERS; i++) {
        waitpid(waiter_pids[i], NULL, 0);
    }
    
    printf("All waiters have finished. Exiting waiter parent process.\n");
    
    return 0;
}