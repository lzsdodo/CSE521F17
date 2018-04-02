#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>
#include <pthread.h>

static void *thread_print_odd(void *vptr_args);
static void *thread_print_even(void *vptr_args);

int main(int argc, char *argv[]) {
	
	pthread_t thread_1;
	pthread_t thread_2;

	if(pthread_create(&thread_1, NULL, thread_print_odd, NULL) != 0) {
		return EXIT_FAILURE;
	}
	
	if(pthread_create(&thread_2, NULL, thread_print_even, NULL) != 0) {
		return EXIT_FAILURE;
	}
	
	if(pthread_join(thread_1, NULL) != 0){
		return EXIT_FAILURE;
	}
	
	if(pthread_join(thread_2, NULL) != 0){
		return EXIT_FAILURE;
	}

	return EXIT_SUCCESS;
}

static void *thread_print_odd(void *vptr_args) {
	int i;
	
	for(i=1; i<=1000; i+=2) {
		printf("%d\n", i);
		usleep(5000);
	}
	
	return NULL;
}

static void *thread_print_even(void *vptr_args) {
	int i;
	
	for(i=2; i<=1000; i+=2) {
		printf("%d\n", i);
		usleep(5000);
	}
	
	return NULL;
}