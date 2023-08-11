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
//#define	SLOTS 140
//#define	SLOTS 126
//#define	SLOTS 64
//#define	SLOTS 32
//#define	SLOTS 96
#define	SLOTS 188
//#define	SLOTS 129
//#define	SLOTS 130 //Freeze issue
//#define	SLOTS 160

//#define SLOT_SIZE SIZE/SLOTS
#define SLOT_SIZE 2*1024*1024*8 //16MB

void verify_linear();
//void verify_random();
int verify_random(char data);


int stage = 65536;
char *list[SLOTS] = {NULL};
char dataArr[SLOTS];

size_t size = SIZE;
size_t slot_size = SLOT_SIZE;
static verify_count = 0;



typedef struct node{
    int value;
    struct node *next;
} node;

node * createNode(int val){
    node *newNode = malloc(sizeof(node));
    newNode->value = val;
    newNode->next = NULL;
    return newNode;
}

void insertEnd(node *head, int val){
    node *newNode = createNode(val);
    node *temp = head;
    while(temp->next)
        temp = temp->next;
    temp->next = newNode;
}


node *reverseAll(node *head){
    if(!head || !head->next) return head;

    node *cur = head;
    node *next = head->next;
    node *prev = NULL;
    while(next){
        cur->next = prev;
        prev = cur;
        cur = next;
        next = next->next;
    }
    cur->next = prev;
    return cur;
}

node *insertFront(node *head, int val){
    node *newNode = createNode(val);
    if(head != NULL){
        newNode->next = head;
    }
    return newNode;
}

void testList(){
	node *head = NULL;
    head = insertFront(head, 30);
    head = insertFront(head, 3110);
    head = insertFront(head, 3120);
    head = insertFront(head, 3310);
    head = insertFront(head, 3140);
    head = insertFront(head, 3510);
    head = insertFront(head, 3610);
    head = insertFront(head, 3170);
    head = insertFront(head, 3810);
    head = insertFront(head, 3190);
    head = insertFront(head, 320);
    head = insertFront(head, 330);
    head = insertFront(head, 340);
    head = insertFront(head, 360);
    head = insertFront(head, 370);
    head = reverseAll(head);
	for(int i=0; i<1650; i++){
		//for(int j=0; j<1599; j++){
		for(int j=0; j<15999; j++){
			int k = 10;
			head = reverseAll(head);
			//printf("A value is %d \n", a);
		}
	}

}


void delay(unsigned int seconds)
{
    clock_t goal = seconds + time(NULL);
    while (goal > time(NULL));
}


int delay_n_usecs(int n){
	popcorn_check_migrate();
	clock_t start, end;
	double cpu_time_used;
//	start = time(NULL);
 
	int a = 100;
	for(int i=0; i<1650; i++){
		//for(int j=0; j<1599; j++){
		for(int j=0; j<15999; j++){
			int k = 10;
			for(int k=0; k<999; k++){
				k = k * 2;
			}
			a++;
			//printf("A value is %d \n", a);
		}
	}
	printf("A value is %d \n", a);
//	end = time(NULL);
//	printf("each delay = %ld seconds \n",(end-start) );
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
		dataArr[i] = data;
	
		while (initialized < SLOT_SIZE) {  //initialize it in batches
			n++;
			memset((char *)list[i] + initialized, data, stage);
			initialized += stage;
		}
		printf("===> slot %d - data %c\n", i, list[i][101200]);
		initialized = 0;
		if(data == 'z')
			data = 'a';
		data = data+1;
	}
	int fail_count = 0;
	data = dataArr[0];
	for(int i=0; i<(SLOTS); i++){
		fail_count = verify_random(data);
	}
		verify_linear();
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
	//if(verify_count < 101 && verify_count%2)
	if(verify_count%2)
	//	delay(1);
	//	delay_n_usecs(8000);
		testList();
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
