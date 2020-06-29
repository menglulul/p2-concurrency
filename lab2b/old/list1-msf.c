/*
 * biglock				no
 * malloc					yes
 * straggler 			yes
 * false sharing 	yes
 */

// input: a table of keys. worker threads allocate nodes prior to insertion

#include <stdio.h>
#include <stdlib.h>
#include <getopt.h>
#include <time.h>
#include <string.h>
#include <assert.h>

#include "SortedList.h"
#include <pthread.h>
#include <signal.h>
#include <errno.h>

#include "common.h"  // xzl
#include "measure.h"  // xzl

//int iterations = 1;
//int numThreads = 1;
//int numList = 1;
//int numparts;
//int mutexFlag = 0;
//int spinLockFlag = 0;


SortedList_t* lists;
//SortedListElement_t* elements;
my_key_t *keys;

int* spinLocks = NULL;
pthread_mutex_t* mutexes = NULL;

int numElements = 0;

struct prog_config the_config;

long long runTime = 0;

#define ONE_BILLION 1000000000L;

void print_errors(char* error){
    if(strcmp(error, "clock_gettime") == 0){
        fprintf(stderr, "Error initializing clock time\n");
        exit(1);
    }
    if(strcmp(error, "thread_create") == 0){
        fprintf(stderr, "Error creating threads.\n");
        exit(2);
    }
    if(strcmp(error, "thread_join") == 0){
        fprintf(stderr, "Error with pthread_join.\n");
        exit(2);
    }
    if(strcmp(error, "mutex") == 0){
        fprintf(stderr, "Error with pthread_join. \n");
        exit(2);
    }
    if(strcmp(error, "segfault") == 0){
        fprintf(stderr, "Segmentation fault caught! \n");
        exit(2);
    }
    if(strcmp(error, "size") == 0){
        fprintf(stderr, "Sorted List length is not zero. List Corrupted\n");
        exit(2);
    }
    if(strcmp(error, "lookup") == 0){
        fprintf(stderr, "Could not retrieve inserted element due to corrupted list.\n");
        exit(2);
    }
    if(strcmp(error, "length") == 0){
        fprintf(stderr, "Could not retrieve length because list is corrupted.\n");
        exit(2);
    }
    if(strcmp(error, "delete") == 0){
        fprintf(stderr, "Could not delete due to corrupted list. \n");
        exit(2);
    }
}

void signal_handler(int sigNum){
    if(sigNum == SIGSEGV){
        print_errors("segfault");
    }
}//signal handler for SIGSEGV

#if 0
// xzl: this only gens 1 char key? bad
//https://stackoverflow.com/questions/19724346/generate-random-characters-in-c generating random characters in C
char* getRandomKeyStr(){
    char* random_key = (char*) malloc(sizeof(char)*2);
    random_key[0] = (char) rand()%26 + 'a';
    random_key[1] = '\0';
    return random_key;
}

my_key_t getRandomKey() {
	my_key_t ret = 0;
	// per byte
	for (unsigned int i = 0; i < sizeof(ret); i++) {
		ret  |= rand() & 0xff;
		ret <<= 8 ;
	}
	return ret;
}

int hashFunctionOld(const char* keys){
    return keys[0]%numList;
}

int hashFunction(my_key_t key){
    return key % numList;
}

#endif

#if 0
void setupLocks(){

    int i;

    mutexes = malloc(sizeof(pthread_mutex_t)*numThreads*1024);

    for(i = 0; i < numThreads*1024; i++){
				if(pthread_mutex_init(&mutexes[i], NULL) < 0){
						print_errors("mutex");
				}
    }

    spinLocks = malloc(sizeof(int)*numparts);
    for (i = 0; i < numparts; i++)
    	spinLocks[i] = 0;

    printf("init %d mutex\n", numThreads);
}


// the resultant sublists
void initializeSubLists(){
    lists = malloc(sizeof(SortedList_t)*numThreads*1024); // padding
    int i;
    for (i = 0; i < numThreads*1024; i++){
        lists[i].prev = &lists[i];
        lists[i].next = &lists[i];
        lists[i].key = 0;
    }

    printf("init %d sub lists\n", numThreads);
}
#endif

void setupLocks(){

    mutexes = malloc(sizeof(pthread_mutex_t)*the_config.numThreads);

    for(int i = 0; i < the_config.numThreads; i++){
				if(pthread_mutex_init(&mutexes[i], NULL) < 0){
						print_errors("mutex");
				}
    }

    spinLocks = malloc(sizeof(int)*the_config.numParts);
    for (int i = 0; i < the_config.numParts; i++)
    	spinLocks[i] = 0;

    printf("init %d mutex\n", the_config.numThreads);
}

void CleanLocks() {

  if (mutexes) {
		for(int i = 0; i < the_config.numThreads; i++)
				pthread_mutex_destroy(&mutexes[i]);
		free(mutexes);
  }

  if (spinLocks)
  	free(spinLocks);

}

void initializeSubLists(){
    lists = malloc(sizeof(SortedList_t) * the_config.numParts);
    for (int i = 0; i < the_config.numThreads; i++){
        lists[i].prev = (SortedListElement_t *)&lists[i];
        lists[i].next = (SortedListElement_t *)&lists[i];
        lists[i].key = 0;
    }

    printf("init %d sub lists. sizeof(SortedList_t) = %lu, seems padding is: %s \n",
    		the_config.numParts, sizeof(SortedList_t),
				sizeof(SortedList_t) > sizeof(SortedListElement_t) ? "ON" : "OFF");
}


void initializeElements(int numElements){
    int i;
    fprintf(stderr, "init %d elements", numElements);

    for(i = 0; i < numElements; i++){
//        elements[i].key = getRandomKey();
    	keys[i] = getRandomKey();
        // debugging
//        if (i < 10)
//        	printf("random keys: %lx \n", elements[i].key);
    }
}


void initializeLists() {
    initializeSubLists();
    setupLocks();
    initializeElements(numElements);
//    splitElements();
}

void* thread_func(void *thread_id) {
	int id = *((int*) thread_id);

	pthread_mutex_lock(&mutexes[0]);
	k2_measure("tr start");
	pthread_mutex_unlock(&mutexes[0]);

	int per_part = numElements / the_config.numThreads;

	for (int i = per_part * id; i < per_part * (id + 1); i++) {

		// we carefully do malloc() w/o grabbing lock
		SortedListElement_t *p = malloc(sizeof(SortedListElement_t));
		assert(p);
		p->key = keys[i];

		SortedList_insert(&lists[id], p);
	}

	pthread_mutex_lock(&mutexes[0]);
	k2_measure("tr done");
	pthread_mutex_unlock(&mutexes[0]);

	return NULL;
}

int main(int argc, char** argv){

		the_config = parse_config(argc, argv);

		int numThreads = the_config.numThreads;
		int iterations = the_config.iterations;
		int numParts = the_config.numParts;

    signal(SIGSEGV, signal_handler);

    k2_measure("init");

    numElements = numThreads * iterations;
//    elements = (SortedListElement_t*) malloc(sizeof(SortedListElement_t)*numElements);
    keys = (my_key_t *)malloc(sizeof(my_key_t) * numElements);
    srand((unsigned int) time(NULL)); //must use srand before rand
    initializeLists();
    
    k2_measure("init done");

    pthread_t threads[numThreads];
    int thread_id[numThreads];
    struct timespec start, end;
    if (clock_gettime(CLOCK_MONOTONIC, &start) < 0){
        print_errors("clock_gettime");
    }
    
    for(int i = 0; i < numThreads; i++){
        thread_id[i] = i;
        int rc = pthread_create(&threads[i], NULL, thread_func, &thread_id[i]);
        if(rc < 0){
            print_errors("thread_create");
        }
    }

    k2_measure("tr launched");

    for(int i = 0; i < numThreads; i++){
        int rc = pthread_join(threads[i], NULL);
        if(rc < 0){
            print_errors("thread_join");
        }
    }
    
    k2_measure("tr joined");

    if(clock_gettime(CLOCK_MONOTONIC, &end) < 0){
        print_errors("clock_gettime");
    }
    
    long long diff = (end.tv_sec - start.tv_sec) * ONE_BILLION;
    diff += end.tv_nsec;
    diff -= start.tv_nsec; //get run time
    
    // we're done. correctness check up
    {
    	long total = 0;
			for(int i = 0; i < numThreads; i++){
					int ll = SortedList_length(&lists[i]);
					fprintf(stderr, "list %d: %d items; ", i, ll);
					total += ll;
			}
			fprintf(stderr, "\ntotal %ld items\n", total);
    }

    k2_measure_flush();

    int numOpts = iterations * numThreads; //get number of operations

    char testname[32];
    getTestName(&the_config, testname, 32);
    print_csv_line(testname, numThreads, iterations, numParts, numOpts, diff);

    // --- clean up ---- //
    CleanLocks();
    free(keys);
    // XXX free individual nodes in @lists XXX
    free(lists);

    exit(0);
}



