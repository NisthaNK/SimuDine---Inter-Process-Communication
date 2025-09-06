#include "ipc_shared.h"
#include <signal.h>
#include <time.h>

// Global IPC identifiers for cleanup
int shmid = -1;
int semid = -1;

// Signal handler for graceful termination
void cleanup_handler(int sig) {
    printf("Restaurant process received signal %d, cleaning up...\n", sig);
    if (shmid != -1) shmctl(shmid, IPC_RMID, NULL);
    if (semid != -1) semctl(semid, 0, IPC_RMID);
    exit(1);
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
    pid_t cook_pid1 = fork();
    if (cook_pid1 == 0) {
        // Cook C
        cmain(0, shmid, semid);
        exit(0);
    }
    
    pid_t cook_pid2 = fork();
    if (cook_pid2 == 0) {
        // Cook D
        cmain(1, shmid, semid);
        exit(0);
    }
    
    // Fork waiter processes
    pid_t waiter_pid1 = fork();
    if (waiter_pid1 == 0) {
        // Waiter U
        wmain(0, shmid, semid);
        exit(0);
    }
    
    pid_t waiter_pid2 = fork();
    if (waiter_pid2 == 0) {
        // Waiter V
        wmain(1, shmid, semid);
        exit(0);
    }
    
    pid_t waiter_pid3 = fork();
    if (waiter_pid3 == 0) {
        // Waiter W
        wmain(2, shmid, semid);
        exit(0);
    }
    
    pid_t waiter_pid4 = fork();
    if (waiter_pid4 == 0) {
        // Waiter X
        wmain(3, shmid, semid);
        exit(0);
    }
    
    pid_t waiter_pid5 = fork();
    if (waiter_pid5 == 0) {
        // Waiter Y
        wmain(4, shmid, semid);
        exit(0);
    }
    
    // Wait for cook processes to finish
    printf("Waiting for cooks to finish...\n");
    waitpid(cook_pid1, NULL, 0);
    waitpid(cook_pid2, NULL, 0);
    
    // Wait for waiter processes to finish
    printf("Waiting for waiters to finish...\n");
    waitpid(waiter_pid1, NULL, 0);
    waitpid(waiter_pid2, NULL, 0);
    waitpid(waiter_pid3, NULL, 0);
    waitpid(waiter_pid4, NULL, 0);
    waitpid(waiter_pid5, NULL, 0);
    
    printf("All processes have finished. Cleaning up IPC resources.\n");
    
    // Clean up IPC resources
    if (shmctl(shmid, IPC_RMID, NULL) == -1) {
        perror("shmctl");
    }
    
    if (semctl(semid, 0, IPC_RMID) == -1) {
        perror("semctl");
    }
    
    printf("Restaurant simulation completed.\n");
    return 0;
}
