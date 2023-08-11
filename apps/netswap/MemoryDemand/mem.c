#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <time.h>
#include <unistd.h>

#define SKIPWARMUP 1

#define SIZE_1M 1024*1024
//#define SIZE_256M 64*1024*1024
//#define SIZE_256M 64*1024*1024
//#define SIZE_256MB 180*1024*1024
//#define SIZE_256MB 210*1024*1024
//#define SIZE_256MB 256*1024*1024
#define SIZE_256MB 256*1024*1024
#define SIZE_512MB 512*1024*1024
#define SIZE_650MB 650*1024*1024
#define SIZE_1GB 1024*1024*1024
#define SIZE_2GB 2*1024*1024*1024

#define SIZE SIZE_2GB
//#define	SLOTS 96
#define	SLOTS 64 // 1GB
//#define	SLOTS 128 // 2GB
//#define	SLOTS 192 // 3GB
//#define	SLOTS 256 // 4GB
//#define	SLOTS 320 // 5GB
//#define	SLOTS 384 // 6GB
//#define	SLOTS 448 // 7GB
//#define	SLOTS 512 // 8GB
//#define	SLOTS 576 // 9GB
//#define	SLOTS 640 // 10GB
//#define	SLOTS 704 // 11GB

//#define SLOT_SIZE SIZE/SLOTS
#define SLOT_SIZE 2*1024*1024*8 //16MB

void verify_linear();
//void verify_random();
int verify_random(char data);
void verify_linear_even();
void verify_linear_odd();
void verify_linear_interval(int start, int interval);

void verify_reverse_linear();
void verify_reverse_linear_even();
void verify_reverse_linear_odd();
void verify_reverse_linear_interval(int start, int interval);


int stage = 65536;
char *list[SLOTS] = {NULL};
char dataArr[SLOTS];

size_t size = SIZE;
size_t slot_size = SLOT_SIZE;
static verify_count = 0;

void delay(unsigned int seconds)
{
    clock_t goal = seconds + time(NULL);
    while (goal > time(NULL));
}


int delay_n_usecs(int n){
	popcorn_check_migrate();
	clock_t start, end;
	double cpu_time_used;
	start = time(NULL);
 
	for(int i=0; i<1650; i++){
		//for(int j=0; j<1599; j++){
		for(int j=0; j<20999; j++){
			int k = 10;
			for(int k=0; k<999; k++){
				k = k * 2;
			}
		}
	}
	end = time(NULL);
	printf("each delay = %ld seconds \n",(end-start) );
	//udelay(n);
	popcorn_check_migrate();
}

int main(void)
{
	clock_t start, end, end_warmup;
	double cpu_time_used;
	start = time(NULL);
    	printf("Total size = %zu bytes ; Slots = %d ; Slot size = %zu bytes\n", size, SLOTS, slot_size);

	//Allocate memory for each element
	for(int i=0; i<SLOTS; i++){
    		list[i] = malloc(slot_size); 
		if (list[i]) {
			printf("Slot %d: Allocated %zu Bytes from %p to %p\n", i, slot_size, list[i], list[i]+slot_size);
		} else {
			printf("Error in allocation.\n");
			return 1;
		}
	}

	//Initialize each element
 	int initialized = 0;
	char data = 'a';
	for(int i=0; i<SLOTS; i++){
		int n = 0;
		printf(" initing = %d \t", i);
	//	if(i == 135)
	//		data = 'o';
		dataArr[i] = data;
	
		while (initialized < SLOT_SIZE) {  //initialize it in batches
			n++;
			memset((char *)list[i] + initialized, data, stage);
			initialized += stage;
		}
#if 0
		int fail_count=0;
		for(int j = 0; j < SLOT_SIZE; j++){
			if(list[i][j] != data)
				fail_count++;
		}
		if(fail_count)
			printf("failed during initializing slab %d \n", i);
//		udelay(100000);
#endif

		//sync();
		printf("===> slot %d - data %c\n", i, list[i][101200]);
		initialized = 0;
		if(data == 'z')
			data = 'a';
		data = data+1;
	}
	int fail_count = 0;
	data = dataArr[0];
	for(int i=0; i<(SLOTS); i++){
	//for(int i=0; i<(150); i++){
	//for(int i=0; i<(100); i++){
		fail_count = verify_random(data);
	}
	//printf(" %d failed out of %d | data = %c \n", fail_count, SLOT_SIZE, data);

	end = time(NULL);
	printf("SANDEEP afn: Time taken: %ld seconds - verified %d \n", (end-start));
	//printf("Time taken: %ld seconds \n", (end-start));
	return 0;
}


int verify_random(char data){
	popcorn_check_migrate(); 
	// Formulae for randomness = ((rand() % (upper - lower + 1)) + lower)
	int test = (rand()%(SLOTS-2+1))+1;
	verify_count++;
	//No delay  // 5 seconds
	if(verify_count%2)
	//if(verify_count < 101)
	//	delay(1);
		delay_n_usecs(8000);
	printf("VERIFICATION: count %d ; SLOT %d;", verify_count, test);
	data = dataArr[test];
	int fail_count = 0;
	//printf("slot = %d \n", test);
	//int temp = 117; 
	//int j = 16688000;
	//printf("Failed at slot %d, element %d, fail_address=%zx, fail_val=%c \n", temp, j, &list[temp][j], list[temp][j]);
	//udelay(200000);
	for(int j = 0; j < SLOT_SIZE; j++){
		if(list[test][j] != data){
			fail_count++;
			//				printf("Failed at slot %d, element %d, fail_address=%zx, fail_val=%c \n", test, j, &list[test][j], list[test][j]);
			//				return;
		}
	}
	printf(" %d failed out of %d | data = %c \n", fail_count, SLOT_SIZE, data);

	popcorn_check_migrate(); 
	return fail_count;
}

void verify_linear_even(){
	popcorn_check_migrate(); 
	int fail_count = 0;
	char data = dataArr[0];
	int temp = 0;
	for(int i=0; i<100; i++){
		// Formulae for randomness = ((rand() % (upper - lower + 1)) + lower)
//		int test = (rand()%(SLOTS-1+1))+1; // Guest page fault
		if(temp >= SLOTS)
			break;
		data = dataArr[temp];
		//printf("slot = %d \n", test);
		//printf("slot = %d \n", temp);
		for(int j = 0; j < SLOT_SIZE; j++){
			if(list[temp][j] != data){
				fail_count++;
			}
		}
		printf("VERIFICATION FAILED: SLOT %d; %d failed out of %d | data = %c \n", temp, fail_count, SLOT_SIZE, data);
//		fail_count = 0;
		temp = temp + 2;
	}
	popcorn_check_migrate(); 
}

void verify_linear_odd(){
	popcorn_check_migrate(); 
	int fail_count = 0;
	char data = dataArr[0];
	int temp = 1;
	for(int i=0; i<100; i++){
		// Formulae for randomness = ((rand() % (upper - lower + 1)) + lower)
//		int test = (rand()%(SLOTS-1+1))+1; // Guest page fault
		if(temp >= SLOTS)
			break;
		data = dataArr[temp];
		//printf("slot = %d \n", test);
		for(int j = 0; j < SLOT_SIZE; j++){
			if(list[temp][j] != data){
				fail_count++;
			}
		}
		printf("VERIFICATION: SLOT %d; %d failed out of %d | data = %c \n", temp, fail_count, SLOT_SIZE, data);
//		fail_count = 0;
		temp = temp + 2;
	}
	popcorn_check_migrate(); 
}

void verify_linear_interval(int start, int interval){
	popcorn_check_migrate(); 
	int fail_count = 0;
	char data = dataArr[0];
	int temp = start;
	for(int i=0; i<100; i++){
		// Formulae for randomness = ((rand() % (upper - lower + 1)) + lower)
//		int test = (rand()%(SLOTS-1+1))+1; // Guest page fault
		if(temp >= SLOTS)
			break;
		data = dataArr[temp];
		//printf("slot = %d \n", test);
		for(int j = 0; j < SLOT_SIZE; j++){
			if(list[temp][j] != data){
				fail_count++;
			}
		}
		printf("VERIFICATION: SLOT %d; %d failed out of %d | data = %c \n", temp, fail_count, SLOT_SIZE, data);
//		fail_count = 0;
		temp = temp + interval;
	}
	popcorn_check_migrate(); 
}

void verify_linear(){
	popcorn_check_migrate(); 
	int fail_count = 0;
	char data = 'a';
	for(int i=0; i<SLOTS; i++){
		for(int j = 0; j < SLOT_SIZE; j++){
			if(list[i][j] != data){
				fail_count++;
			}
		}
		printf("VERIFICATION: SLOT %d; %d failed out of %d | data = %c \n", i, fail_count, SLOT_SIZE, data);
//		fail_count = 0;
		if(data == 'z')
			data = 'a';
		data = data+1;
	}
	popcorn_check_migrate(); 
}

void verify_reverse_linear_even(){
	popcorn_check_migrate(); 
	int fail_count = 0;
	char data = dataArr[0];
	int temp = SLOTS-1;
	for(int i=0; i<100; i++){
		// Formulae for randomness = ((rand() % (upper - lower + 1)) + lower)
//		int test = (rand()%(SLOTS-1+1))+1; // Guest page fault
		if(temp < 0)
			break;
		data = dataArr[temp];
		printf("slot = %d \n", temp);
		for(int j = 0; j < SLOT_SIZE; j++){
			if(list[temp][j] != data){
				fail_count++;
			}
		}
		printf("VERIFICATION FAILED: SLOT %d; failed %d out of %d | data = %c \n", temp, fail_count, SLOT_SIZE, data);
//		fail_count = 0;
		temp = temp - 2;
	}
	popcorn_check_migrate(); 
}

void verify_reverse_linear_odd(){
	popcorn_check_migrate(); 
	int fail_count = 0;
	char data = dataArr[0];
	int temp = SLOTS-2;
	for(int i=0; i<100; i++){
		// Formulae for randomness = ((rand() % (upper - lower + 1)) + lower)
//		int test = (rand()%(SLOTS-1+1))+1; // Guest page fault
		if(temp < 0)
			break;
		data = dataArr[temp];
		//printf("slot = %d \n", test);
		for(int j = 0; j < SLOT_SIZE; j++){
			if(list[temp][j] != data){
				fail_count++;
			}
		}
		printf("VERIFICATION FAILED: SLOT %d; failed %d out of %d | data = %c \n", temp, fail_count, SLOT_SIZE, data);
//		fail_count = 0;
		temp = temp - 2;
	}
	popcorn_check_migrate(); 
}

void verify_reverse_linear_interval(int start, int interval){
	popcorn_check_migrate(); 
	int fail_count = 0;
	char data = dataArr[start];
	int temp = start;
	for(int i=0; i<100; i++){
		// Formulae for randomness = ((rand() % (upper - lower + 1)) + lower)
//		int test = (rand()%(SLOTS-1+1))+1; // Guest page fault
		if(temp < 0)
			break;
		data = dataArr[temp];
		//printf("slot = %d \n", test);
		for(int j = 0; j < SLOT_SIZE; j++){
			if(list[temp][j] != data){
				fail_count++;
			}
		}
		printf("VERIFICATION FAILED: SLOT %d; failed %d out of %d | data = %c \n", temp, fail_count, SLOT_SIZE, data);
//		fail_count = 0;
		temp = temp - interval;
	}
	popcorn_check_migrate(); 
}

void verify_reverse_linear(){
	popcorn_check_migrate(); 
	int fail_count = 0;
	char data = dataArr[SLOTS-1];
	int temp = SLOTS-1;
	for(int i=SLOTS-1; i>=0; i--){
		data = dataArr[temp];
		for(int j = 0; j < SLOT_SIZE; j++){
			if(list[i][j] != data){
				fail_count++;
			}
		}
		printf("VERIFICATION FAILED: SLOT %d; failed %d out of %d | data = %c \n", i, fail_count, SLOT_SIZE, data);
//		fail_count = 0;
		temp--;
	}
	popcorn_check_migrate(); 
}
