/* 
 * CSE 421/521 Assignment 0, Fall 2017, Universtiy at Buffalo
 * 
 * This is TA Sharath's code, which solves the problem using Conditional Variables
 *
 */
#include <stdio.h>
#include <pthread.h>
#include <semaphore.h>
#define MAX 1001

void *print_even(void *);
void *print_odd(void *);

pthread_mutex_t mutex = PTHREAD_MUTEX_INITIALIZER;
pthread_cond_t cv = PTHREAD_COND_INITIALIZER;
int flag_g = 0;

int main()
{
	pthread_t pid[2];
	int *ret_value = NULL;

	pthread_create(&pid[0], NULL, print_even, NULL);
	pthread_create(&pid[1], NULL, print_odd, NULL);
	pthread_join(pid[0], (void*)ret_value);
	pthread_join(pid[1], (void*)ret_value);
	return 0;
}

void *print_even(void *a)
{
	int i = 0;
	printf("%d\n", i);
	pthread_mutex_lock(&mutex);
	if (flag_g == 0) {
		pthread_cond_wait(&cv, &mutex);
	}
	pthread_cond_signal(&cv);
	i += 2;
	while (i < MAX){
		pthread_cond_wait(&cv, &mutex);
		printf("%d\n", i);
		pthread_cond_signal(&cv);
		i += 2;
	}
	pthread_mutex_unlock(&mutex);
	return NULL;
}

void *print_odd(void *a)
{
	int i = 1;
	pthread_mutex_lock(&mutex);
	flag_g = 1;
	pthread_cond_signal(&cv);
	while (i < MAX){
		pthread_cond_wait(&cv, &mutex);
		printf("%d\n", i);
		pthread_cond_signal(&cv);
		i += 2;
	}
	pthread_mutex_unlock(&mutex);
	return NULL;
}