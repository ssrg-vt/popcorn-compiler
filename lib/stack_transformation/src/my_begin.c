#include <stdio.h>

#include <my_begin.h>
#include <errno.h>
#include <my_private.h>
//#include <my_errors.h>

static const Elf _elf_init = INIT_ELF;
static const char fmag[] = ARFMAG;

static int
my_xread(int fd, char *buffer, size_t len) {
    size_t done = 0;
    size_t n;

    while (done < len) {
	n = read(fd, buffer + done, len - done);
	if (n == 0) {
	    /* premature end of file */
	    return -1;
	}
	else if (n != (size_t)-1) {
	    /* some bytes read, continue */
	    done += n;
	}
	else if (errno != EAGAIN && errno != EINTR) {
	    /* real error */
	    return -1;
	}
    }
    return 0;
}

void*
my__elf_read(Elf *elf, void *buffer, size_t off, size_t len) {
    void *tmp;

    if (elf->e_disabled) {
//	seterr(ERROR_FDDISABLED);
    }
    else if (len) {
	off += elf->e_base;
	if (lseek(elf->e_fd, (off_t)off, SEEK_SET) != (off_t)off) {
//	    seterr(ERROR_IO_SEEK);
	}
	else if (!(tmp = buffer) && !(tmp = malloc(len))) {
//	    seterr(ERROR_IO_2BIG);
	}
	else if (my_xread(elf->e_fd, tmp, len)) {
//	    seterr(ERROR_IO_READ);
	    if (tmp != buffer) {
		free(tmp);
	    }
	}
	else {
	    return tmp;
	}
    }
    return NULL;
}

static unsigned long
my_getnum(const char *str, size_t len, int base, size_t *err) {
    unsigned long result = 0;

    while (len && *str == ' ') {
	str++; len--;
    }
    while (len && *str >= '0' && (*str - '0') < base) {
	result = base * result + *str++ - '0'; len--;
    }
    while (len && *str == ' ') {
	str++; len--;
    }
    if (len) {
	*err = len;
    }
    return result;
}

static void
my__elf_init_ar(Elf *elf) {
    struct ar_hdr *hdr;
    size_t offset;
    size_t size;
    size_t err = 0;

    elf->e_kind = ELF_K_AR;
    elf->e_idlen = SARMAG;
    elf->e_off = SARMAG;

    /* process special members */
    offset = SARMAG;
    while (!elf->e_strtab && offset + sizeof(*hdr) <= elf->e_size) {
	hdr = (struct ar_hdr*)(elf->e_data + offset);
	if (memcmp(hdr->ar_fmag, fmag, sizeof(fmag) - 1)) {
	    break;
	}
	if (hdr->ar_name[0] != '/') {
	    break;
	}
	size = my_getnum(hdr->ar_size, sizeof(hdr->ar_size), 10, &err);
	if (err || !size) {
	    break;
	}
	offset += sizeof(*hdr);
	if (offset + size > elf->e_size) {
	    break;
	}
	if (hdr->ar_name[1] == '/' && hdr->ar_name[2] == ' ') {
	    elf->e_strtab = elf->e_data + offset;
	    elf->e_strlen = size;
	    break;
	}
	if (hdr->ar_name[1] != ' ') {
	    break;
	}
	/*
	 * Windows (.lib) archives provide two symbol tables
	 * The first one is the one we want.
	 */
	if (!elf->e_symtab) {
	    elf->e_symtab = elf->e_data + offset;
	    elf->e_symlen = size;
	}
	offset += size + (size & 1);
    }
}

static void
my__elf_check_type(Elf *elf, size_t size) {
    elf->e_idlen = size;
    if (size >= EI_NIDENT && !memcmp(elf->e_data, ELFMAG, SELFMAG)) {
	elf->e_kind = ELF_K_ELF;
	elf->e_idlen = EI_NIDENT;
	elf->e_class = elf->e_data[EI_CLASS];
	elf->e_encoding = elf->e_data[EI_DATA];
	elf->e_version = elf->e_data[EI_VERSION];
    }
    else if (size >= SARMAG && !memcmp(elf->e_data, ARMAG, SARMAG)) {
	printf("Look like an AR\n");
   	my__elf_init_ar(elf);
    }
}

Elf*
my_read_elf_begin(int fd, Elf_Cmd cmd, Elf *ref) {
    size_t size = 0;
    off_t off;
    Elf *elf;

    //elf_assert(_elf_init.e_magic == ELF_MAGIC);
    /*if (_elf_version == EV_NONE) {
	seterr(ERROR_VERSION_UNSET);
	return NULL;
    }*/
    if (cmd == ELF_C_NULL) {
	return NULL;
    }
    else if (cmd == ELF_C_WRITE) {
		ref = NULL;
    }
    else if (cmd != ELF_C_READ && cmd != ELF_C_RDWR) {
		//seterr(ERROR_INVALID_CMD);
	return NULL;
    }
    else if ((off = lseek(fd, (off_t)0, SEEK_END)) == (off_t)-1
	  || (off_t)(size = off) != off) {
	//seterr(ERROR_IO_GETSIZE);
	return NULL;
    }

    if (!(elf = (Elf*)malloc(sizeof(Elf)))) {
//	seterr(ERROR_MEM_ELF);
	return NULL;
    }
    *elf = _elf_init;
    elf->e_fd = fd;
    elf->e_parent = ref;
    elf->e_size = elf->e_dsize = size;

    if (cmd != ELF_C_READ) {
	elf->e_writable = 1;
    }
    if (cmd != ELF_C_WRITE) {
	elf->e_readable = 1;
    }
    else {
		return elf;
    }

    if (ref) {
    }
    else if (size) {
	if (!(elf->e_data = my__elf_read(elf, NULL, 0, size))) {
	    free(elf);
		printf("7. Here\n");
	    return NULL;
	}
    }

    my__elf_check_type(elf, size);
    return elf;
}
