/* 
 * CSE 421/521 Assignment 0, Fall 2017, Universtiy at Buffalo
 * 
 * This is TA Sharath's code, which solves the problem using semaphores
 *
 */
#include <stdio.h>
#include <pthread.h>
#include <semaphore.h>

void *print_even(void *);
void *print_odd(int *);
sem_t mutex_odd_printed, mutex_even_printed;

int main()
{
	pthread_t pid[2];
	int *ret_value = NULL;
	sem_init(&mutex_odd_printed, 0, 0);
	sem_init(&mutex_even_printed, 0, 0);
	pthread_create(&pid[0], NULL, print_even, NULL);
	pthread_create(&pid[1], NULL, (void *(*)(void*))print_odd, NULL);
	pthread_join(pid[0], (void*)ret_value);
	pthread_join(pid[1], (void*)ret_value);
	return 0;
}

void *print_even(void *a)
{
	int i = 0;
	printf("%d\n", i);
	sem_post(&mutex_even_printed);
	i += 2;
	while (i < 25){
		sem_wait(&mutex_odd_printed);
		printf("%d\n", i);
		sem_post(&mutex_even_printed);
		i += 2;
	}
	return NULL;
}

void *print_odd(int *a)
{
	int i = 1;
	while (i < 25){
		sem_wait(&mutex_even_printed);
		printf("%d\n", i);
		sem_post(&mutex_odd_printed);
		i += 2;
	}
	return NULL;
}