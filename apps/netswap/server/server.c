//#include <fcntl.h>
#include <netinet/in.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/socket.h>
#include <unistd.h>
#include <inttypes.h>
#include <errno.h>

//extern __thread int vcpufd;
extern uint8_t* guest_mem;

#define PAGE_BITS 12
#define PAGE_SIZE 4096
#define FREE_START 0x4400
//#define TAB_SIZE 689500 //Works from 0x4400 to 0xAA000
//#define TAB_SIZE 800000 //Works from 0x4400 to 0xAA000
#define TAB_SIZE 2097151
//0xc04010e0
#define PORT 7080

size_t virt_table[TAB_SIZE] = {NULL};
static unsigned int pg_idx = 0;
static int count = 0;

/* Popcorn data structures for on demand memory transfer */
typedef enum {
       PFAULT_FATAL = 0,
       PFAULT_HEAP,
       PFAULT_BSS,
       PFAULT_DATA
} pfault_type_t;

struct packet {
        pfault_type_t type;
        uint64_t address;
        uint8_t npages;
        uint32_t page_size;
};

int receive_memdis_request(int server, pfault_type_t *type,
                uint64_t *addr, uint8_t *npages, uint64_t *page_size) {
        int valread;
        struct packet recv_packet;

        // Read received data
        valread = read(server , &recv_packet, sizeof(struct packet));
        if(valread != sizeof(struct packet)) {
                if(valread == 0 && (errno == ENOENT || errno == EINTR)) {
                        printf("!! Client exited\n");
                        //memdis_server_running = 0;
                        return -1;
                }

                //err(EXIT_FAILURE, "swapIn count=%d, failed/short read (%d, shoud be %d) on page request "
                //               "reception", count, valread, sizeof(struct packet));
                printf("swapIn count=%d, failed/short read (%d, shoud be %d) on page request reception \n", count, valread, sizeof(struct packet));
                return -1;
        }

        *type = recv_packet.type;
        *addr = recv_packet.address;
        *npages = recv_packet.npages;
        *page_size = recv_packet.page_size;

        return 0;
}
void start_memdis_server(){
        int server_fd, new_socket;
        struct sockaddr_in address;
        int opt = 1;
        int addrlen = sizeof(address);
        size_t size=0;

        pfault_type_t req_type;
        uint64_t req_addr;
        uint8_t npages;
        uint64_t npages_64;
        uint64_t page_size = 4096;

        int valread;

//        printf("Available memory is: %lu \n",mem_avail());
        if ((server_fd = socket(AF_INET, SOCK_STREAM, 0)) == 0)
                err(EXIT_FAILURE, "Socket Failed");

        if (setsockopt(server_fd, SOL_SOCKET, SO_REUSEADDR | SO_REUSEPORT,
                        &opt, sizeof(opt)))
                err(EXIT_FAILURE, "Setsockopt Failed");

        address.sin_family = AF_INET;
	address.sin_addr.s_addr = INADDR_ANY;
        address.sin_port = htons(PORT);

        if (bind(server_fd, (struct sockaddr *)&address, sizeof(address))<0)
                err(EXIT_FAILURE, "Bind Failed");

reconnect:
        if (listen(server_fd, 3) < 0)
                        err(EXIT_FAILURE, "Listen Failed");

        printf("Remote page server listenning on port %d...\n", PORT);
     //   fflush(stdout);

        if ((new_socket = accept(server_fd, (struct sockaddr *)&address, (socklen_t*)&addrlen))<0)
                err(EXIT_FAILURE, "Accept Failed");
        printf("Client connected!\n");
        fflush(stdout);

        uint64_t *addr;
	//int np = 256;
	int np = 4096;
//        char* buffer = malloc(PAGE_SIZE*SLAB_PG); //Slab size buffer
        char* buffer = malloc(np*PAGE_SIZE); // Page size buffer
	int count1=0;
        while(1) {
                if(receive_memdis_request(new_socket, &req_type, &req_addr, &npages, &page_size) == -1)
                        goto reconnect;
  //              printf("Received Virtual addr is 0x%zx -- %lx \n ", req_addr, req_addr);
		npages_64 = page_size;
                int page_sz = 4096;
                if(req_type == PFAULT_HEAP){ //Receive slab
			
			unsigned int idx=0;
			//printf("Receving viraddr:%zx; npages=%d\n", req_addr, npages);
                      	
			idx = ((req_addr>>PAGE_BITS));
			//idx = ((req_addr>>PAGE_BITS) - FREE_START);
			char *test;
			test = (char *)malloc(npages_64*page_sz); // Alocating slab size
			printf(" ==== Swapping out: viraddr:%zx; idx=%d; npages=%d\n", req_addr, idx, npages_64);
			if(test != NULL){
				for(int i=0; i<npages_64; i++){
					virt_table[idx+i] = test + i*page_sz;
				}
			}
			else
				printf("Already alloated!! \n");
                        char *paddr = virt_table[idx];
                        int total_sz = npages_64 * page_sz;
			while(size < total_sz)  {
				valread = recv(new_socket ,(void*)(buffer+size), total_sz-size, 0);
				if(valread == -1) {
					perror("!! !@ Page receive failed");
					return -1;
				}
				size += valread;
			}

			memcpy(paddr, buffer, size);
			//printf("Buffer[100] = %c \n", buffer[100]);
			size = 0;
		}
		else if(req_type == PFAULT_BSS){
#if 1
			unsigned int idx=0;
			int test1=0;
		//	if(npages == 0) npages_64 = 256;
		//	else npages_64 = npages;
			//npages_64 = 1;
			char * send_buf = (char *) malloc(npages_64*page_sz);
			idx = ((req_addr>>PAGE_BITS));
			//idx = ((req_addr>>PAGE_BITS) - FREE_START);
			char *test;
			size_t vaddr = NULL;
			printf("Swap in: count: %d;  address=%zx, npages = %d \n", count, req_addr, npages_64);
			count++;
			for(int i=0; i<npages_64; i++){
				vaddr = virt_table[idx+i];
				memcpy(send_buf + i*page_sz, vaddr, page_sz); // Send a slab 
			}
			send(new_socket,(const void*) send_buf, npages_64*page_sz, 0);
			free(send_buf);
#endif
                }

        }
        free(buffer);
        close(server_fd);
        close(new_socket);
}

int main(int argc, const char *argv[])
{
	//printf("Available memory is \n ");	
	start_memdis_server();
}

