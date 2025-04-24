// Lab 6 for Operating Systems
// Christopher Strand

#include <cstdlib>
#include <pthread.h> // needed for pthread_t, pthread_create, pthread_join
#include <unistd.h> // needed for sleep function
#include <iostream> // needed for printf function
#include <mutex> // needed for mutex
#include <time.h> // needed for srand
#include <chrono>
#include <atomic>

using namespace std;

using chrono::high_resolution_clock;
using chrono::duration_cast;
using chrono::duration;
using chrono::milliseconds;

// This allows # of philosophers to be easily adjusted
// Changes: Increased Number of Philosophers
const int num_of_philosophers = 5;
bool chopsticks[num_of_philosophers];
// Changes: This is checkings the current state of each philosopher
// 0: no chopsticks, 1: just right chopstick, 2: just left chopstick, 3: both chopsticks
int philosopher_state[num_of_philosophers] {0};
//

bool philosopher_attempting[num_of_philosophers] {false};

int pid_id[num_of_philosophers];
bool map[num_of_philosophers];

// This allows thinking/eating time mins and maxes to be adjustable. Please always have max>min and never allow min to be less than 1
// Changes: Decreased max time for eating and thinking so they're all the same time
const int glob_think_time = 0;
const int glob_eat_time = 0;

const int max_think_time = 10;
const int min_think_time = 0;
const int max_eat_time = 10;
const int min_eat_time = 0;

// Changes: create a max number of think/eat loops
const int max_loops = 1500;

bool deadlock = false;

mutex c[num_of_philosophers]; // lock for chopsticks[]
mutex p; // lock of philosopher_state[]
mutex o; // lock for all printf statements

atomic<int> time_eating = 0;
atomic<int> time_thinking = 0;
atomic<int> time_waiting = 0;
int total_time = 0;

// Changes: Checking if all chopsticks are taken
int chopstick_check(void){
    
    for (int i = 0; i < num_of_philosophers; i++){
        if (chopsticks[i] == false){
            return 1;
        }
    }

    return 0;
}

// Changes: Checking if all philosophers are in the same state
// Returns -1 if deadlock, else returns 0
int philosopher_state_check(void){
    int same_state = 0;
    lock_guard<mutex> lock(p);
    for (int i = 0; i < num_of_philosophers; i++){
        // if a philosopher has no chopsticks or two chopsticks
        if (philosopher_state[i] == 0 or philosopher_state[i] == 3){
            return 0;
        // if the state of the first philosopher has not been saved yet
        } else if (same_state == 0){
            same_state = philosopher_state[i];
        // if a subsequent philosopher doesn't have the same state
        } else if( philosopher_state[i] != same_state){
            return 0;
        }
    }
    
    return -1;
}

// create the container of chopsticks. If they are not held, then they will equal false
int initialize_chopsticks(void){
    for (int i = 0; i < num_of_philosophers; i++){
        chopsticks[i] = false;
    }
    if (chopsticks == NULL){
        return -1;
    }
    return 1;
}

// create the pid (numbering for philosophers) and the map to insures philosophers don't end up with the same name
int initialize_map(void){
    for (int i = 0; i < num_of_philosophers; i++){
        pid_id[i] = i + 1;
        map[i] = false;
    }
    if (pid_id == NULL or map == NULL){
        return -1;
    }
    return 1;
}

int allocate_pid(void){
    for (int i = 0; i < num_of_philosophers; i++){
        if (!map[i]){
            map[i] = true;
            return pid_id[i];
        }
    }
    return -1;
}

void *runner(void *param){
    p.lock();
    int pid = allocate_pid(); // # or name of philosopher
    p.unlock();
    int personal_chopsticks[2]; // index of chopsticks on either side of philosopher
    int neighbors[2]; // container that holds the pids of this philosopher's right ([0]) and left ([1]) neighbors
    
    //
    //
    personal_chopsticks[0] = pid - 1; // assign index of right chopstick
    if (pid == 1){
        neighbors[0] = num_of_philosophers;
    } else {
        neighbors[0] = pid - 1;
    }
    if (pid == num_of_philosophers){ // assign index for left chopstick
        personal_chopsticks[1] = 0;
        neighbors[1] = 1;
    } else {
        personal_chopsticks[1] = pid;
        neighbors[1] = pid + 1;
    }
    
    // Change: When done eating, go back to thinking, in a loop
    int loops = 0;
    do {
        //
        // Change: When max_loops is exceeded, exit the loop
        loops += 1;
        if (loops > max_loops){
            /*
            o.lock();
            printf("Philosopher %d has exited the loop.\n", pid);
            o.unlock();
            */
            break;
        }
        //
        //
        // int thinktime = glob_think_time;
        int thinktime = rand() % (max_think_time - min_think_time) + min_think_time;

        //
        //
        /*
        o.lock();
        printf("Philosopher %d will think for %d microseconds.\n", pid, thinktime);
        o.unlock();
        */

        //
        //
        usleep(thinktime);
        time_thinking += thinktime;

        auto t1 = high_resolution_clock::now();

        do {
            /*
            // Change: check if any chopsticks are free
            if (chopstick_check() == 1){
            */
                // attempt is which chopstick will be attempted 0=right, 1=left
                int attempt; 
                if (philosopher_state[pid-1] == 1){ // if right hand has chopstick, try to grab left hand chopstick
                    attempt = 1;
                } else if (philosopher_state[pid-1] == 2){ // if left had has chopstick, try to grab right hand chopstick
                    attempt = 0;
                } else {
                    attempt = ((rand() % 2)); // randomly determines if left or right hand chopstick will be attempted
                }
                //
                //
                p.lock();
                if (philosopher_attempting[neighbors[0]-1] == false and philosopher_attempting[neighbors[1]-1] == false){
                    philosopher_attempting[pid-1] = true;
                }
                p.unlock();


                if (philosopher_attempting[pid-1] == true){
                    if (attempt == 0){
                        // if their neighbor to the right already has the chopstick to their right (or both)
                        if ((philosopher_state[neighbors[0]-1] == 1 and philosopher_state[pid-1] == 0) or philosopher_state[neighbors[0]-1] == 3){
                            attempt = -1;
                        }
                    } else {
                        // if their neighbor to the left already has the chopstick to their left
                        if ((philosopher_state[neighbors[1]-1] == 2 and philosopher_state[pid-1] == 0) or philosopher_state[neighbors[1]-1] == 3){
                            attempt = -1;
                        }
                    }
                    // critical section, the checking for and possibly picking up of the chopstick
                    //
                    //
                    if (attempt != -1){
                        c[personal_chopsticks[attempt]].lock();
                        if (chopsticks[personal_chopsticks[attempt]] == false){
                            chopsticks[personal_chopsticks[attempt]] = true;
                            
                            // if right chopstick was obtained
                            if (attempt == 0){
                                // and left chopstick was already obtained
                                if (philosopher_state[pid-1] == 2){
                                    philosopher_state[pid-1] = 3;
                                // and left chopstick was NOT already obtained
                                } else {
                                    philosopher_state[pid-1] = 1;
                                }
                            // if left chopstick was obtained
                            } else {
                                // and right chopstick was already obtained
                                if (philosopher_state[pid-1] == 1){
                                    philosopher_state[pid-1] = 3;
                                // and right chopstick was NOT already obtained
                                } else {
                                    philosopher_state[pid-1] = 2;
                                }
                            }
                            //
                            //
                            /*
                            o.lock();
                            printf("Philosopher %d picked up chopstick %d ", pid, personal_chopsticks[attempt]);
                            if (attempt == 0){
                                printf("with their right hand.\n");
                            } else {
                                printf("with their left hand.\n");
                            }
                            o.unlock();
                            */
                        }
                        c[personal_chopsticks[attempt]].unlock();
                    }
                    p.lock();
                    philosopher_attempting[pid-1] = false;
                    p.unlock();
                }

            // if the function flagged that all philosophers are holding 1 chopstick
            /*
            } else if (philosopher_state_check() == -1){
                o.lock();
                    if (deadlock == false){
                        deadlock = true;
                        printf("We have a deadlock!\n");
                    }
                o.unlock();  
            }
            */
        } while (philosopher_state[pid-1] != 3 and deadlock == false); // must have both chopsticks to eat   
        
        auto t2 = high_resolution_clock::now();
        auto ms_int = duration_cast<milliseconds>(t2 - t1);
        time_waiting += ms_int.count();

        if (deadlock == false){
            // int eattime = glob_eat_time;
            int eattime = rand() % (max_eat_time - min_eat_time) + min_eat_time;

            //
            /*
            o.lock();
            printf("Philosopher %d is holding chopsticks %d and %d. Philisopher will now eat for %d microseconds.\n", pid, personal_chopsticks[0], personal_chopsticks[1], eattime);
            o.unlock();
            */

            //
            usleep(eattime); // Time for philosopher to eat
            //
            time_eating += eattime;

            // for good measure, locking access to chopsticks while the philosopher who has finished eating is putting them down.
            c[personal_chopsticks[0]].lock();
            chopsticks[personal_chopsticks[0]] = false;
            c[personal_chopsticks[0]].unlock();
            c[personal_chopsticks[1]].lock();
            chopsticks[personal_chopsticks[1]] = false;
            c[personal_chopsticks[1]].unlock();
            
            p.lock();
            philosopher_state[pid-1] = 0;
            p.unlock();

            /*
            o.lock();
            printf("Philosopher %d has finished eating and placed chopsticks %d and %d down.\n", pid, personal_chopsticks[0], personal_chopsticks[1]);
            o.unlock();
            */
        } else {
            break;
        }
        
    } while (true); 
    return NULL;
}

int main(){
    srand(time(NULL)); // initialize random function
    if(initialize_chopsticks() == -1){
        printf("Failed to initialize chopsticks.\n");
        return 1;
    }
    if(initialize_map() == -1){
        printf("Failed to initialize map.\n");
        return 2;
    }

    pthread_t philosphers[num_of_philosophers];

    auto t1 = high_resolution_clock::now();

    for (int i = 0; i < num_of_philosophers; i++){
        pthread_create(&philosphers[i], NULL, runner, NULL);
    }

    for (int i = 0; i < num_of_philosophers; i++){
        pthread_join(philosphers[i], NULL);
    }

    auto t2 = high_resolution_clock::now();
    auto ms_int = duration_cast<milliseconds>(t2 - t1);
    total_time = ms_int.count();

    printf("Total time in simulation (ms)... %d\n", (int)total_time);
    printf("Total time waiting (ms)... %d\n", (int)time_waiting);
    printf("Total time eating (ms)... %d\n", (int)time_eating/1000);
    printf("Total time thinking (ms)... %d\n", (int)time_thinking/1000);
    return 0;
}