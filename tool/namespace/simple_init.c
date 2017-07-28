/* simple_init.c

   Copyright 2013, Michael Kerrisk
   Licensed under GNU General Public License v2 or later

   A simple init(1)-style program to be used as the init program in
   a PID namespace. The program reaps the status of its children and
   provides a simple shell facility for executing commands.
*/
#define _GNU_SOURCE
#include <unistd.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <signal.h>
#include <wordexp.h>
#include <errno.h>
#include <sys/wait.h>

#define errExit(msg)    do { perror(msg); exit(EXIT_FAILURE); \
                        } while (0)

static int verbose = 0;

/* Display wait status (from waitpid() or similar) given in 'status' */

/* SIGCHLD handler: reap child processes as they change state */

static void
child_handler(int sig)
{
    pid_t pid;
    int status;

    /* WUNTRACED and WCONTINUED allow waitpid() to catch stopped and
       continued children (in addition to terminated children) */

    while ((pid = waitpid(-1, &status,
                          WNOHANG | WUNTRACED | WCONTINUED)) != 0) {
        if (pid == -1) {
            if (errno == ECHILD)        /* No more children */
                break;
            else
                perror("waitpid");      /* Unexpected error */
        }

        if (verbose)
            printf("\tinit: SIGCHLD handler: PID %ld terminated\n",
                    (long) pid);
    }
}

/* Perform word expansion on string in 'cmd', allocating and
   returning a vector of words on success or NULL on failure */

static char **
expand_words(char *cmd)
{
    char **arg_vec;
    int s;
    wordexp_t pwordexp;

    s = wordexp(cmd, &pwordexp, 0);
    if (s != 0) {
        fprintf(stderr, "Word expansion failed\n");
        return NULL;
    }

    arg_vec = calloc(pwordexp.we_wordc + 1, sizeof(char *));
    if (arg_vec == NULL)
        errExit("calloc");

    for (s = 0; s < pwordexp.we_wordc; s++)
        arg_vec[s] = pwordexp.we_wordv[s];

    arg_vec[pwordexp.we_wordc] = NULL;

    return arg_vec;
}

static void
usage(char *pname)
{
    fprintf(stderr, "Usage: %s [-q]\n", pname);
    fprintf(stderr, "\t-v\tProvide verbose logging\n");

    exit(EXIT_FAILURE);
}

int
main(int argc, char *argv[])
{
    struct sigaction sa;
#define CMD_SIZE 10000
    char cmd[CMD_SIZE];
    pid_t pid;
    int opt;

    while ((opt = getopt(argc, argv, "v")) != -1) {
        switch (opt) {
        case 'v': verbose = 1;          break;
        default:  usage(argv[0]);
        }
    }

    sa.sa_flags = SA_RESTART | SA_NOCLDSTOP;
    sigemptyset(&sa.sa_mask);
    sa.sa_handler = child_handler;
    if (sigaction(SIGCHLD, &sa, NULL) == -1)
        errExit("sigaction");

    if (verbose)
        printf("\tinit: my PID is %ld\n", (long) getpid());

    /* Performing terminal operations while not being the foreground
       process group for the terminal generates a SIGTTOU that stops the
       process.  However our init "shell" needs to be able to perform
       such operations (just like a normal shell), so we ignore that
       signal, which allows the operations to proceed successfully. */

    signal(SIGTTOU, SIG_IGN);

    /* Become leader of a new process group and make that process
       group the foreground process group for the terminal */

    if (setpgid(0, 0) == -1)
        errExit("setpgid");;
    if (tcsetpgrp(STDIN_FILENO, getpgrp()) == -1)
        errExit("tcsetpgrp-child");

    if (! access("./init", X_OK)) {
	pid = fork();
        if (pid == -1)
            errExit("fork");

	if (pid == 0) {
	    execvp("./init", NULL);
            errExit("execvp");
        }
    }

    while (1) {

        /* Read a shell command; exit on end of file */

        printf("simple_init$ ");
        if (fgets(cmd, CMD_SIZE, stdin) == NULL) {
            if (verbose)
                printf("\tinit: exiting");
            printf("\n");
            exit(EXIT_SUCCESS);
        }

        if (cmd[strlen(cmd) - 1] == '\n')
            cmd[strlen(cmd) - 1] = '\0';        /* Strip trailing '\n' */

        if (strlen(cmd) == 0)
            continue;           /* Ignore empty commands */

        pid = fork();           /* Create child process */
        if (pid == -1)
            errExit("fork");

        if (pid == 0) {         /* Child */
            char **arg_vec;

            arg_vec = expand_words(cmd);
            if (arg_vec == NULL)        /* Word expansion failed */
                continue;

            /* Make child the leader of a new process group and
               make that process group the foreground process
               group for the terminal */

            if (setpgid(0, 0) == -1)
                errExit("setpgid");;
            if (tcsetpgrp(STDIN_FILENO, getpgrp()) == -1)
                errExit("tcsetpgrp-child");

            /* Child executes shell command and terminates */

            execvp(arg_vec[0], arg_vec);
            errExit("execvp");          /* Only reached if execvp() fails */
        }

        /* Parent falls through to here */

        if (verbose)
            printf("\tinit: created child %ld\n", (long) pid);

        pause();                /* Will be interrupted by signal handler */

        /* After child changes state, ensure that the 'init' program
           is the foreground process group for the terminal */

        if (tcsetpgrp(STDIN_FILENO, getpgrp()) == -1)
            errExit("tcsetpgrp-parent");
    }
}
