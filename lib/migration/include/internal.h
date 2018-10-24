/*
 * Internal functions used by the library.
 *
 * Author: Rob Lyerly <rlyerly@vt.edu>
 * Date: 3/4/2018
 */

#ifndef _INTERNAL_H
#define _INTERNAL_H

/*
 * Because the kernel can't restore some registers through the syscall return
 * path, these functions provide a little bit of architecture-specific glue
 * code to restore them manually.
 */

void __migrate_fixup_aarch64(void);
void __migrate_fixup_x86_64(void);

#endif /* _INTERNAL_H */

