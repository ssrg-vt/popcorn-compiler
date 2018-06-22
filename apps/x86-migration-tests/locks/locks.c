#include <stdio.h>
#include <pthread.h>
#include <hermit/migration.h>



int main(void) {
	pthread_mutex_t mutex;
	
	int ret = pthread_mutex_init(&mutex, NULL);
	printf("%d\n", ret);

	printf("Starting experiment\n");
	pthread_mutex_lock(&mutex);

	HERMIT_FORCE_MIGRATION();

	//pthread_mutex_lock(&mutex);
	pthread_mutex_unlock(&mutex);

	pthread_mutex_destroy(&mutex);
	printf("Done\n");
	return 0;
}
