#include <stdio.h>
#include <stdlib.h>
#include <pthread.h>
#include <unistd.h>
#include <stdbool.h>

// Semaphore structure
typedef struct {
    int value;
    pthread_mutex_t mtx;
    pthread_cond_t cv;
} semaphore;

// Initialize semaphore
void P(semaphore *s) {
    pthread_mutex_lock(&s->mtx);
    while (s->value <= 0)
        pthread_cond_wait(&s->cv, &s->mtx);
    s->value--;
    pthread_mutex_unlock(&s->mtx);
}

void V(semaphore *s) {
    pthread_mutex_lock(&s->mtx);
    s->value++;
    pthread_cond_signal(&s->cv);
    pthread_mutex_unlock(&s->mtx);
}

// Shared data
int leftvisitors;
int m, n;
bool *BA; // boat available
int *BC, *BT; // boat capacity, boat time
semaphore boat, rider;
pthread_mutex_t bmtx;
pthread_barrier_t EOS, *BB;

void *boat_thread(void *arg) {
    int id = *(int *)arg;
    printf("Boat %d Ready\n", id+1);
    while (1) {
        V(&rider);
        P(&boat);
        pthread_mutex_lock(&bmtx);
        BA[id] = true;
        BC[id] = -1;
        pthread_mutex_unlock(&bmtx);
        pthread_barrier_wait(&BB[id]);
        pthread_mutex_lock(&bmtx);
        int visitor = BC[id];
        int rtime = BT[id];
        BA[id] = false;
        pthread_mutex_unlock(&bmtx);
        printf("Boat %d Start of ride for visitor %d\n", id+1, visitor+1);
        usleep(rtime * 100000);
        printf("Boat %d End of ride for visitor %d (ride time = %d)\n", id+1, visitor+1,rtime);
        pthread_mutex_lock(&bmtx);
        leftvisitors--;
        if(leftvisitors == 0) {
            pthread_mutex_unlock(&bmtx);
            pthread_barrier_wait(&EOS);
            break;
        }
        pthread_mutex_unlock(&bmtx);
    }
    return NULL;
}

void *visitor_thread(void *arg) {
    int id = *(int *)arg;
    int vtime = rand() % 91 + 30;
    int rtime = rand() % 46 + 15;
    printf("Visitor %d Starts sightseeing for %d minutes\n", id+1, vtime);
    usleep(vtime * 100000);
    printf("Visitor %d Ready to ride a boat (ride time = %d)\n", id+1, rtime);
    V(&boat);
    P(&rider);
    int found_boat = -1;
    while (found_boat == -1) {
        pthread_mutex_lock(&bmtx);
        for (int i = 0; i < m; i++) {
            if (BA[i] && BC[i] == -1) {
                BC[i] = id;
                BT[i] = rtime;
                found_boat = i;
                break;
            }
        }
        pthread_mutex_unlock(&bmtx);
        if (found_boat == -1)
            usleep(10000);
    }
    printf("Visitor %d Finds boat %d\n", id+1, found_boat+ 1);
    pthread_barrier_wait(&BB[found_boat]);
    usleep(rtime * 100000);
    printf("Visitor %d Leaving \n", id+1);
    return NULL;
}

int main(int argc, char *argv[]) {
    if (argc != 3) {
        fprintf(stderr, "Usage: %s <boats> <visitors>\n", argv[0]);
        exit(1);
    }
    
    m = atoi(argv[1]);
    n = atoi(argv[2]);

    leftvisitors = n;
    
    pthread_t boat_threads[m], visitor_threads[n];
    int boat_ids[m], visitor_ids[n];
    
    boat = (semaphore){0, PTHREAD_MUTEX_INITIALIZER, PTHREAD_COND_INITIALIZER};
    rider = (semaphore){0, PTHREAD_MUTEX_INITIALIZER, PTHREAD_COND_INITIALIZER};
    pthread_mutex_init(&bmtx, NULL);
    pthread_barrier_init(&EOS, NULL, 2);
    
    BA = calloc(m, sizeof(bool));
    BC = calloc(m, sizeof(int));
    BT = calloc(m, sizeof(int));
    BB = malloc(m * sizeof(pthread_barrier_t));
    
    for (int i = 0; i < m; i++) {
        BA[i] = false;
        BC[i] = -1;
        pthread_barrier_init(&BB[i], NULL, 2);
    }
    
    for (int i = 0; i < m; i++) {
        boat_ids[i] = i;
        pthread_create(&boat_threads[i], NULL, boat_thread, &boat_ids[i]);
    }
    
    for (int i = 0; i < n; i++) {
        visitor_ids[i] = i;
        pthread_create(&visitor_threads[i], NULL, visitor_thread, &visitor_ids[i]);
    }
    
    pthread_barrier_wait(&EOS);
    printf("All visitors have left\n");
    for (int i = 0; i < n; i++) pthread_join(visitor_threads[i], NULL);
    for (int i = 0; i < m; i++) pthread_cancel(boat_threads[i]);
    
    free(BA);
    free(BC);
    free(BT);
    free(BB);
    pthread_mutex_destroy(&bmtx);
    pthread_barrier_destroy(&EOS);
    return 0;
}
