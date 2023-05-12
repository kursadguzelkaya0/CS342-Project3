#include <stdio.h>
#include <pthread.h>
#include <stdlib.h>
#include "rm.h"

int DA; // indicates if deadlocks will be avoided or not
int N; // number of processes
int M; // number of resource types
int ExistingRes[MAXR]; // Existing resources vector

//..... other definitions/variables .....
int AvailableRes[MAXR]; // Available resources vector
int Allocation[MAXP][MAXR]; 
int Request[MAXP][MAXR]; 
int MaxDemand[MAXP][MAXR]; 
int Need[MAXP][MAXR]; 

pthread_cond_t** thread_cv_arr;

/* Declare mutex variable */
pthread_mutex_t availableRes_lock;

long int self_to_tid[MAXP];

// TODO: Works only if tids are also index checkk!!
int getTid(long int self) {
    for (int i = 0; i < MAXP; i++) {
        if (self_to_tid[i] == self) {
            return i;
        }
    }
    return -1;
}

int rm_thread_started(int tid) {
    self_to_tid[tid] = pthread_self();
    return 0;
}

int rm_thread_ended() {
    self_to_tid[getTid(pthread_self())] = -1;
    return 0;
}

int rm_claim (int claim[]) {
    for(int i = 0; i < M; i++) {
        if (claim[i] > ExistingRes[i]) {
            //printf("A process can not claim more than the total number of a resource instance!!");
            return -1;
        }
        if ( DA == 1) {
            MaxDemand[getTid(pthread_self())][i] = claim[i];
            Need[getTid(pthread_self())][i] = claim[i];
        }
        else {
            MaxDemand[getTid(pthread_self())][i] = 0;
            Need[getTid(pthread_self())][i] = 0;
        }
    }
    return 0;
}

int rm_init(int p_count, int r_count, int r_exist[], int avoid) {
    int i;

    DA = avoid;
    N = p_count;
    M = r_count;

    // Check fail conditions
    if (N > MAXP || M > MAXR || N < 0 || M < 0 || !(avoid == 0 || avoid == 1)) {
        //printf("Process or resource count can not be negative or more than max value!");
        return -1;
    }

    // initialize (create) resources
    for (i = 0; i < M; ++i) {
        ExistingRes[i] = r_exist[i];
        AvailableRes[i] = r_exist[i];
    }
    // resources initialized (created)

    for (i = 0; i < N; ++i) {
        for (int j = 0; j < M; ++j) {
            Need[i][j] = 0;
            Allocation[i][j] = 0; 
            Request[i][j] = 0; 
            MaxDemand[i][j] = 0; 
        }
    }

    /* Initialize mutex lock for availableRes */
    pthread_mutex_init(&availableRes_lock, NULL);

    thread_cv_arr = (pthread_cond_t**)malloc(N * sizeof(pthread_cond_t*));

    for (int i = 0; i < N; i++) {
        thread_cv_arr[i] = (pthread_cond_t*) malloc(sizeof(pthread_cond_t));
    }
    //....
    // ...
    return 0;
}

int safety_check() {
    int Work[MAXR];
    int Finish[MAXP];

    for (int i = 0; i < MAXR; i++) {
        Work[i] = AvailableRes[i];
    }

    for (int k = 0; k < MAXP; k++) {
        Finish[k] = 0;
    }

    for (int k = 0; k < MAXP; k++) {
        if (Finish[k] == 0) {
            int canFinish = 1;
            for (int i = 0; i < MAXR; i++) {
                if (Need[k][i] > Work[i]) {
                    canFinish = 0;
                    break;
                }
            }
            if (canFinish == 1) {
                Finish[k] = 1;
                for (int i = 0; i < MAXR; i++) {
                    Work[i] += Allocation[k][i];
                }
                k = -1; // start over
            }
        }
    }

    for (int a = 0; a < MAXP; a++) {
        if (Finish[a] == 0) {
            return -1; // unsafe state
        }
    }
    return 1; // safe state
}


int rm_request (int request[]) {

    if ( DA == 1) {
        
        pthread_mutex_lock(&availableRes_lock);

        //Step by Step Algorithm from slides
        //1: If Request ≤ Need , go to step 2. Otherwise, raise error condition,
        //since process has exceeded its maximum claim.
        for( int i = 0; i< M; i++ ) { //todo: direkt request mi check etmeliyim belki bu check asagı da eklenebilir.
           if ( Request[getTid(pthread_self())][i] + request[i] + Allocation[getTid(pthread_self())][i]> MaxDemand[getTid(pthread_self())][i] ) {
                //printf("Number of requested resources leads to exceeding of the maximum demand.");
                pthread_mutex_unlock(&availableRes_lock);
                return -1; 
           }

        }
        for( int i = 0; i< M; i++ ) { 
            Request[getTid(pthread_self())][i] += request[i];
        }

     
        //2 If requesti < available i

        while (1) {
            int enoughResInsCount = 0;

            for (int i = 0; i < M; i++) {
                    if (AvailableRes[i] >= request[i]) {
                           enoughResInsCount++;
                    } else {
                        break;
                    }
            }
            if (enoughResInsCount < M) {
                    // wait for signal on condition variable
                    pthread_cond_wait((thread_cv_arr[getTid(pthread_self())]), &availableRes_lock); 
            }
            else {
                //MUTEX LOCK BIRAKMAYI UNUTMA
                 for (int i = 0; i < M; i++) {
                    AvailableRes[i] -= request[i];
                    Allocation[getTid(pthread_self())][i] += request[i];
                    Need[getTid(pthread_self())][i] -= request[i];
                }
                int isSafe = safety_check();
                if (isSafe == 1) {
                    for ( int i = 0; i <M; i++ ) {
                        Request[getTid(pthread_self())][i] -= request[i]; 
                    }

                    for (int j = 0; j < N; j++ ) {
                        pthread_cond_signal(thread_cv_arr[j]);
                    }
                    pthread_mutex_unlock(&availableRes_lock);
                    break;

                }
                else {
                    for (int i = 0; i < M; i++) {
                        AvailableRes[i] += request[i];
                        Allocation[getTid(pthread_self())][i] -= request[i];
                        Need[getTid(pthread_self())][i] += request[i];

                    }
                    //printf("Waiting for state to be safe %d!\n", getTid(pthread_self()));
                    pthread_cond_wait((thread_cv_arr[getTid(pthread_self())]), &availableRes_lock); 

                }
            }
        }
        return 0;
    } else {
        pthread_mutex_lock(&availableRes_lock);

        for( int i = 0; i< M; i++ ) { //todo: direkt request mi check etmeliyim belki bu check asagı da eklenebilir.
           if ( Request[getTid(pthread_self())][i] + request[i] + Allocation[getTid(pthread_self())][i]> ExistingRes[i] ) {
                //printf("Number of requested resources leads to exceed number of the existing resources.");
                pthread_mutex_unlock(&availableRes_lock);
                return -1; 
           }
        }
        for( int i = 0; i< M; i++ ) { 
            Request[getTid(pthread_self())][i] += request[i];
        }
        
        while (1) {
            int enoughResInsCount = 0;
            /* Lock availableRes before accessing it */
            // Check for source availability
            for (int i = 0; i < M; i++) {
                if (AvailableRes[i] >= request[i]) {
                    enoughResInsCount++;
                } else {
                    break;
                }
            }

            if (enoughResInsCount < M) {
                // wait for signal on condition variable
                // TODO: wait conditional ??
                pthread_cond_wait((thread_cv_arr[getTid(pthread_self())]), &availableRes_lock);

            } else {
                //printf("resource allocated\n");
                // Allocate resources
                for (int i = 0; i < M; i++) {
                    AvailableRes[i] -= request[i];
                    Allocation[getTid(pthread_self())][i] += request[i];
                    Request[getTid(pthread_self())][i] -= request[i]; // TODO: bida bak buna 

                    /* Unlock availableRes after accessing it */
                    pthread_mutex_unlock(&availableRes_lock);
                }

                break;
            }
        }
    }

    return 0;
}


int rm_release (int release[]) {
    /* Lock availableRes before accessing it */
    pthread_mutex_lock(&availableRes_lock);
    // Check for source availability
    for (int i = 0; i < M; i++) {
       if (Allocation[getTid(pthread_self())][i] < release[i]) {
            //printf("Process can not relase more instance of a resource than it allocated!");
            pthread_mutex_unlock(&availableRes_lock);
            return -1;
        }

        if (release[i] > 0) {
            Allocation[getTid(pthread_self())][i] -= release[i];
            AvailableRes[i] += release[i];
            
            for (int j = 0; j < N; j++ ) {
                pthread_cond_signal(thread_cv_arr[j]);
            }

        }
    }

    pthread_mutex_unlock(&availableRes_lock);
    return 0;
}

int rm_detection() {

    int WorkRes[MAXR]; // represents the available pool
    int Finished[MAXP]; 

    /* Lock availableRes before accessing it */
    pthread_mutex_lock(&availableRes_lock);

    for (int i = 0; i < M; i++) {
        WorkRes[i] = AvailableRes[i];
    }

    // Step 1
    for (int i = 0; i < N; i++) {
        int isFinished = 1;
        for (int j = 0; j < M; j++) {
            if (Allocation[i][j] != 0) {
                isFinished = 0;
                break;
            }
        }
        Finished[i] = isFinished;
    }

    // Step 2
    int found;
    do {
        found = 0;
        for (int i = 0; i < N; i++) {
            if (Finished[i] != 1) {
                int can_run = 1;
                for (int j = 0; j < M; j++) {
                    if (Request[i][j] > WorkRes[j]) {
                        can_run = 0;
                        break;
                    }
                }
                if (can_run) { // Step 3
                    found = 1;
                    for (int j = 0; j < M; j++) {
                        WorkRes[j] += Allocation[i][j];
                    }
                    Finished[i] = 1;
                }
            }
        }
    } while (found);

    // Step 4: Check if there are any deadlocked processes
    int num_deadlocked = 0;
    for (int i = 0; i < N; i++) {
        if (!Finished[i]) {
            num_deadlocked++;
        }
    }

    pthread_mutex_unlock(&availableRes_lock);
    return num_deadlocked;
}

void rm_print_state(char headermsg[]) {
    int i, j;
    
    printf("##########################\n");
    printf("%s\n", headermsg);
    printf("##########################\n");

    printf("Exist:\n");
    printf(" ");
    for (i = 0; i < M; i++) {
        printf(" R%d", i);
    }
    printf("\n");
    printf(" ");
    for (i = 0; i < M; i++) {
        printf(" %d", ExistingRes[i]);
    }
    printf("\n");

    printf("Available:\n");
    printf(" ");
    for (i = 0; i < M; i++) {
        printf(" R%d", i);
    }
    printf("\n");
    printf(" ");
    for (i = 0; i < M; i++) {
        printf(" %d", AvailableRes[i]);
    }
    printf("\n");

    printf("Allocation:\n");
    printf(" ");
    for (i = 0; i < M; i++) {
        printf(" R%d", i);
    }
    printf("\n");
    for (i = 0; i < N; i++) {
        printf("T%d:", i);
        for (j = 0; j < M; j++) {
            printf(" %d", Allocation[i][j]);
        }
        printf("\n");
    }

    printf("Request:\n");
    printf(" ");
    for (i = 0; i < M; i++) {
        printf(" R%d", i);
    }
    printf("\n");
    for (i = 0; i < N; i++) {
        printf("T%d:", i);
        for (j = 0; j < M; j++) {
            printf(" %d", Request[i][j]);
        }
        printf("\n");
    }

    printf("MaxDemand:\n");
    printf(" ");
    for (i = 0; i < M; i++) {
        printf(" R%d", i);
    }
    printf("\n");
    for (i = 0; i < N; i++) {
        printf("T%d:", i);
        for (j = 0; j < M; j++) {
            printf(" %d", MaxDemand[i][j]);
        }
        printf("\n");
    }

    printf("Need:\n");
    printf(" ");
    for (i = 0; i < M; i++) {
        printf(" R%d", i);
    }
    printf("\n");
    for (i = 0; i < N; i++) {
        printf("T%d:", i);
        for (j = 0; j < M; j++) {
            printf(" %d", Need[i][j]);
        }
        printf("\n");
    }
    printf("###########################\n");
}
