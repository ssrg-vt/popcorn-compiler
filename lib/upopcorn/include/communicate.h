#pragma once

#include "unistd.h"
//#include "common.h"
//#include "config.h"

/* Max characters to encode a number */
/* Note also update the server! */
#define str(s) #s
#define NUM_LINE_SIZE_BUF_STRING str(NUM_LINE_SIZE_BUF)
#define NUM_LINE_SIZE_BUF 20

/* default port */
#define DEFAULT_PORT 9999


#define CMD_SIZE 4
#define ARG_SIZE_SIZE NUM_LINE_SIZE_BUF

enum comm_cmd{
	GET_PAGE = 0,
	PRINT_ST,
	GET_CTXT,
	GET_PMAP,
	SND_EXIT,
};

static char *comm_cmd_char[] = {
	"GET_PAGE",
	"PRINT_ST"
	"GET_CTXT",
	"GET_PMAP",
	"SND_EXIT",
};

int comm_migrate(int nid);
int send_cmd(enum comm_cmd cmd, int size, char *arg);
int send_cmd_rsp(enum comm_cmd cmd, int size, char *arg, int resp_size, void* resp);
int send_data(void* addr, size_t len);


