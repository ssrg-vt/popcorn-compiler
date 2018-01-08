/*
 * strptr.c - implementation of the elf_strptr(3) function.
 * Copyright (C) 1995 - 2007 Michael Riepe
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

#include <my_private.h>

#ifndef lint
//static const char rcsid[] = "@(#) $Id: strptr.c,v 1.12 2008/05/23 08:15:35 michael Exp $";
#endif /* lint */
#if 0
static Elf_Data*
my__elf_cook_scn(Elf *elf, Elf_Scn *scn, Scn_Data *sd) {
    Elf_Data dst;
    Elf_Data src;
    int flag = 0;
    size_t dlen;

    elf_assert(elf->e_data);

    /*
     * Prepare source
     */
    src = sd->sd_data;
    src.d_version = elf->e_version;
    if (elf->e_rawdata) {
	src.d_buf = elf->e_rawdata + scn->s_offset;
    }
    else {
	src.d_buf = elf->e_data + scn->s_offset;
    }

    /*
     * Prepare destination (needs prepared source!)
     */
    dst = sd->sd_data;
    if (elf->e_class == ELFCLASS64) {
	dlen = _elf64_xltsize(&src, dst.d_version, elf->e_encoding, 0);
    }
    else {
	elf_assert(valid_class(elf->e_class));
	seterr(ERROR_UNIMPLEMENTED);
	return NULL;
    }
    if (dlen == (size_t)-1) {
	return NULL;
    }
    dst.d_size = dlen;
    if (elf->e_rawdata != elf->e_data && dst.d_size <= src.d_size) {
	dst.d_buf = elf->e_data + scn->s_offset;
    }
    else if (!(dst.d_buf = malloc(dst.d_size))) {
	seterr(ERROR_MEM_SCNDATA);
	return NULL;
    }
    else {
	flag = 1;
    }

    /*
     * Translate data
     */
    if (_elf_xlatetom(elf, &dst, &src)) {
	sd->sd_memdata = (char*)dst.d_buf;
	sd->sd_data = dst;
	if (!(sd->sd_free_data = flag)) {
	    elf->e_cooked = 1;
	}
	return &sd->sd_data;
    }

    if (flag) {
	free(dst.d_buf);
    }
    return NULL;
}
#endif

Elf_Data*
my_elf_getdata(Elf_Scn *scn, Elf_Data *data) {
    Scn_Data *sd;
    Elf *elf;

    if (!scn) {
	return NULL;
    }
    elf_assert(scn->s_magic == SCN_MAGIC);
    if (scn->s_index == SHN_UNDEF) {
//	seterr(ERROR_NULLSCN);
    }
    else if (data) {
	for (sd = scn->s_data_1; sd; sd = sd->sd_link) {
	    elf_assert(sd->sd_magic == DATA_MAGIC);
	    elf_assert(sd->sd_scn == scn);
	    if (data == &sd->sd_data) {
		/*
		 * sd_link allocated by elf_newdata().
		 */
		return &sd->sd_link->sd_data;
	    }
	}
//	seterr(ERROR_SCNDATAMISMATCH);
    }
    else if ((sd = scn->s_data_1)) {
	elf_assert(sd->sd_magic == DATA_MAGIC);
	elf_assert(sd->sd_scn == scn);
	elf = scn->s_elf;
	elf_assert(elf);
	elf_assert(elf->e_magic == ELF_MAGIC);
	if (sd->sd_freeme) {
	    /* allocated by elf_newdata() */
	    return &sd->sd_data;
	}
	else if (scn->s_type == SHT_NULL) {
	    //seterr(ERROR_NULLSCN);
	}
	else if (sd->sd_memdata) {
	    /* already cooked */
	    return &sd->sd_data;
	}
	else if (scn->s_offset < 0 || scn->s_offset > elf->e_size) {
	    //seterr(ERROR_OUTSIDE);
	}
	else if (scn->s_type == SHT_NOBITS || !scn->s_size) {
	    /* no data to read */
	    return &sd->sd_data;
	}
	else if (scn->s_offset + scn->s_size > elf->e_size) {
	    //seterr(ERROR_TRUNC_SCN);
	}
/*	else if (valid_class(elf->e_class)) {
	    return _elf_cook_scn(elf, scn, sd);
	}
*/	else {
	   // seterr(ERROR_UNKNOWN_CLASS);
	}
    }
    return NULL;
}

Elf_Scn*
my_elf_getscn(Elf *elf, size_t index) {
    Elf_Scn *scn;

    if (!elf) {
	return NULL;
    }
    elf_assert(elf->e_magic == ELF_MAGIC);
    if (elf->e_kind != ELF_K_ELF) {
	//seterr(ERROR_NOTELF);
    }
    else if (elf->e_ehdr) {
	for (scn = elf->e_scn_1; scn; scn = scn->s_link) {
	    elf_assert(scn->s_magic == SCN_MAGIC);
	    elf_assert(scn->s_elf == elf);
	    if (scn->s_index == index) {
		return scn;
	    }
	}
	//seterr(ERROR_NOSUCHSCN);
    }
    return NULL;
}

char*
my_elf_strptr(Elf *elf, size_t section, size_t offset) {
    Elf_Data *data;
    Elf_Scn *scn;
    size_t n;
    char *s;

    if (!elf) {
	return NULL;
    }
    elf_assert(elf->e_magic == ELF_MAGIC);
    if (!(scn = my_elf_getscn(elf, section))) {
	return NULL;
    }
    if (scn->s_index == SHN_UNDEF) {
//	seterr(ERROR_NOSTRTAB);
	return NULL;
    }
    /*
     * checking the section header is more appropriate
     */
    if (elf->e_class == ELFCLASS32) {
	if (scn->s_shdr32.sh_type != SHT_STRTAB) {
	   // seterr(ERROR_NOSTRTAB);
	    return NULL;
	}
    }
#if __LIBELF64
    else if (elf->e_class == ELFCLASS64) {
	if (scn->s_shdr64.sh_type != SHT_STRTAB) {
	    //seterr(ERROR_NOSTRTAB);
	    return NULL;
	}
    }
#endif /* __LIBELF64 */
    else if (valid_class(elf->e_class)) {
	//seterr(ERROR_UNIMPLEMENTED);
	return NULL;
    }
    else {
	//seterr(ERROR_UNKNOWN_CLASS);
	return NULL;
    }
    /*
     * Find matching buffer
     */
    n = 0;
    data = NULL;
    if (elf->e_elf_flags & ELF_F_LAYOUT) {
	/*
	 * Programmer is responsible for d_off
	 * Note: buffers may be in any order!
	 */
	while ((data = my_elf_getdata(scn, data))) {
	    n = data->d_off;
	    if (offset >= n && offset - n < data->d_size) {
		/*
		 * Found it
		 */
		break;
	    }
	}
    }
    else {
	/*
	 * Calculate offsets myself
	 */
	while ((data = my_elf_getdata(scn, data))) {
	    if (data->d_align > 1) {
		n += data->d_align - 1;
		n -= n % data->d_align;
	    }
	    if (offset < n) {
		/*
		 * Invalid offset: points into a hole
		 */
		//seterr(ERROR_BADSTROFF);
		return NULL;
	    }
	    if (offset - n < data->d_size) {
		/*
		 * Found it
		 */
		break;
	    }
	    n += data->d_size;
	}
    }
    if (data == NULL) {
	/*
	 * Not found
	 */
	//seterr(ERROR_BADSTROFF);
	return NULL;
    }
    if (data->d_buf == NULL) {
	/*
	 * Buffer is NULL (usually the programmers' fault)
	 */
	//seterr(ERROR_NULLBUF);
	return NULL;
    }
    offset -= n;
    s = (char*)data->d_buf;
//    if (!(_elf_sanity_checks & SANITY_CHECK_STRPTR)) {
    if (!SANITY_CHECK_STRPTR) {
	return s + offset;
    }
    /*
     * Perform extra sanity check
     */
    for (n = offset; n < data->d_size; n++) {
	if (s[n] == '\0') {
	    /*
	     * Return properly NUL terminated string
	     */
	    return s + offset;
	}
    }
    /*
     * String is not NUL terminated
     * Return error to avoid SEGV in application
     */
    //seterr(ERROR_UNTERM);
    return NULL;
}
