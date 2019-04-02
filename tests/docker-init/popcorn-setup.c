#include <fcntl.h>
#include <unistd.h>
#include <errno.h>
#include <stdio.h>
#include <arch.h>
#include <sys/stat.h>



static int cp(const char *to, const char *from)
{
    int fd_to, fd_from;
    char buf[4096];
    ssize_t nread;
    int saved_errno;

    fd_from = open(from, O_RDONLY);
    if (fd_from < 0)
        return -1;

    fd_to = open(to, O_WRONLY | O_CREAT | O_TRUNC, 0666);
    if (fd_to < 0)
        goto out_error;

    while (nread = read(fd_from, buf, sizeof buf), nread > 0)
    {
        char *out_ptr = buf;
        ssize_t nwritten;

        do {
            nwritten = write(fd_to, out_ptr, nread);

            if (nwritten >= 0)
            {
                nread -= nwritten;
                out_ptr += nwritten;
            }
            else if (errno != EINTR)
            {
                goto out_error;
            }
        } while (nread > 0);
    }

    if (nread == 0)
    {
        if (close(fd_to) < 0)
        {
            fd_to = -1;
            goto out_error;
        }
        close(fd_from);

        /* Success! */
        return 0;
    }

  out_error:
    saved_errno = errno;

    close(fd_from);
    if (fd_to >= 0)
        close(fd_to);

    errno = saved_errno;
    return -1;
}

static char* get_ext_string()
{
	char *carch=NULL;
	enum arch ret = popcorn_get_current_arch();
	switch(ret) {
		case ARCH_AARCH64:
			carch="aarch64";
			break;
		case ARCH_X86_64:
			carch="x86-64";
			break;
		default:
			carch="unknown";
			break;
	};
	//printf("using compile time %s\n", ARCHEXT);
	//printf("using run time %s\n", carch);
	return carch;
}

int cp(const char *to, const char *from);
char* get_ext_string();
void popcorn_setup(char* binary)
{
	#define MAX_PATH 1024
	char* arch_ext;
	char binary_arch[MAX_PATH];

	if (!binary)
		return;
	
	arch_ext=get_ext_string();	

	//TODO:check that path fit in MAX_PATH
	sprintf(binary_arch, "%s_%s", binary, arch_ext);

	//printf("copying from %s to %s\n", binary_arch, binary);
	
	int ret=cp(binary, (char*)binary_arch);
	if(ret)
		perror("Could not copy binary!\n");

	chmod(binary, 0777);

	return;
}
