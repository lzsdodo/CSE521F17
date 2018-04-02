#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>
#include <pthread.h>

pthread_mutex_t count_mutex     = PTHREAD_MUTEX_INITIALIZER;
pthread_mutex_t condition_mutex = PTHREAD_MUTEX_INITIALIZER;
pthread_cond_t  condition_cond  = PTHREAD_COND_INITIALIZER;

void *print_num_1(void);
void *print_num_2(void);
int count = 1;

int main(int argc, char **argv) {
	
	pthread_t thread_1, thread_2;
	int iret_1, iret_2;
	
	iret_1 =  pthread_create(&thread_1, NULL, print_num_1, NULL);
	iret_2 =  pthread_create(&thread_2, NULL, print_num_2, NULL);	
	pthread_join(thread_1, NULL);
	pthread_join(thread_2, NULL);

	return EXIT_SUCCESS;
}


void *print_num_1(void) {
	for(;;) {
		pthread_mutex_lock(&condition_mutex);
		pthread_cond_wait(&condition_cond, &condition_mutex);
		pthread_mutex_unlock(&condition_mutex);

		pthread_mutex_lock(&count_mutex);
		printf("%d\n",count);
		count++;
		pthread_mutex_unlock(&count_mutex);

		if(count >= 1000) return(NULL);
	}
}

void *print_num_2(void) {
	for(;;)	{
		pthread_mutex_lock(&condition_mutex);
		pthread_cond_signal(&condition_cond);
		pthread_mutex_unlock(&condition_mutex);

		pthread_mutex_lock(&count_mutex);
		printf("%d\n",count);
		count++;
		pthread_mutex_unlock(&count_mutex);

		if(count >= 1000) return(NULL);
	}
}



