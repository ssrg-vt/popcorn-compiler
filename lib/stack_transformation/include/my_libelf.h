/*
 * libelf.h - public header file for libelf.
 * Copyright (C) 1995 - 2008 Michael Riepe
 * 
 * This library is free software; you can redistribute it and/or
 * modify it under the terms of the GNU Library General Public
 * License as published by the Free Software Foundation; either
 * version 2 of the License, or (at your option) any later version.
 * 
 * This library is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the GNU
 * Library General Public License for more details.
 * 
 * You should have received a copy of the GNU Library General Public
 * License along with this library; if not, write to the Free Software
 * Foundation, Inc., 51 Franklin Street, Fifth Floor, Boston, MA 02110-1301, USA
 */

/* @(#) $Id: libelf.h,v 1.29 2009/07/07 17:57:43 michael Exp $ */

#ifndef _LIBELF_H
#define _LIBELF_H

#include <stddef.h>	/* for size_t */
#include <sys/types.h>

#if __LIBELF_INTERNAL__
#include <my_sys_elf.h>
#else /* __LIBELF_INTERNAL__ */
//#include <libelf/sys_elf.h>
#endif /* __LIBELF_INTERNAL__ */

#if defined __GNUC__ && !defined __cplusplus
#define DEPRECATED	__attribute__((deprecated))
#else
#define DEPRECATED	/* nothing */
#endif

#ifdef __cplusplus
extern "C" {
#endif /* __cplusplus */

#ifndef __P
# if (__STDC__ + 0) || defined(__cplusplus) || defined(_WIN32)
#  define __P(args) args
# else /* __STDC__ || defined(__cplusplus) */
#  define __P(args) ()
# endif /* __STDC__ || defined(__cplusplus) */
#endif /* __P */

/*
 * Commands
 */
typedef enum {
    ELF_C_NULL = 0,	/* must be first, 0 */
    ELF_C_READ,
    ELF_C_WRITE,
    ELF_C_CLR,
    ELF_C_SET,
    ELF_C_FDDONE,
    ELF_C_FDREAD,
    ELF_C_RDWR,
    ELF_C_NUM		/* must be last */
} Elf_Cmd;

/*
 * Flags
 */
#define ELF_F_DIRTY	0x1
#define ELF_F_LAYOUT	0x4
/*
 * Allow sections to overlap when ELF_F_LAYOUT is in effect.
 * Note that this flag ist NOT portable, and that it may render
 * the output file unusable.  Use with extreme caution!
 */
#define ELF_F_LAYOUT_OVERLAP	0x10000000

/*
 * File types
 */
typedef enum {
    ELF_K_NONE = 0,	/* must be first, 0 */
    ELF_K_AR,
    ELF_K_COFF,
    ELF_K_ELF,
    ELF_K_NUM		/* must be last */
} Elf_Kind;

/*
 * Data types
 */
typedef enum {
    ELF_T_BYTE = 0,	/* must be first, 0 */
    ELF_T_ADDR,
    ELF_T_DYN,
    ELF_T_EHDR,
    ELF_T_HALF,
    ELF_T_OFF,
    ELF_T_PHDR,
    ELF_T_RELA,
    ELF_T_REL,
    ELF_T_SHDR,
    ELF_T_SWORD,
    ELF_T_SYM,
    ELF_T_WORD,
    /*
     * New stuff for 64-bit.
     *
     * Most implementations add ELF_T_SXWORD after ELF_T_SWORD
     * which breaks binary compatibility with earlier versions.
     * If this causes problems for you, contact me.
     */
    ELF_T_SXWORD,
    ELF_T_XWORD,
    /*
     * Symbol versioning.  Sun broke binary compatibility (again!),
     * but I won't.
     */
    ELF_T_VDEF,
    ELF_T_VNEED,
    ELF_T_NUM		/* must be last */
} Elf_Type;

/*
 * Elf descriptor
 */
typedef struct Elf	Elf;

/*
 * Section descriptor
 */
typedef struct Elf_Scn	Elf_Scn;

/*
 * Archive member header
 */
typedef struct {
    char*		ar_name;
    time_t		ar_date;
    long		ar_uid;
    long 		ar_gid;
    unsigned long	ar_mode;
    off_t		ar_size;
    char*		ar_rawname;
} Elf_Arhdr;

/*
 * Archive symbol table
 */
typedef struct {
    char*		as_name;
    size_t		as_off;
    unsigned long	as_hash;
} Elf_Arsym;

/*
 * Data descriptor
 */
typedef struct {
    void*		d_buf;
    Elf_Type		d_type;
    size_t		d_size;
    off_t		d_off;
    size_t		d_align;
    unsigned		d_version;
} Elf_Data;

#endif
