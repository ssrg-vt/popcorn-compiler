#include <stddef.h>
#include "pthread_impl.h"
#include "libc.h"

__attribute__((__visibility__("hidden")))
void *__tls_get_new(tls_mod_off_t *);

void *__tls_get_addr(tls_mod_off_t *v)
{
	pthread_t self = __pthread_self();
	if (v[0]<=(size_t)self->dtv[0]) {
		/*
		 * BUGFIX explained. I ran into a TLS bug that was
		 * triggered by the fact that our compile and link
		 * options force use to use the LDTLS model. Usually
		 * when musl-libc is making static-pie binaries it
		 * still uses the IE/LD TLS model. In our case we
		 * are building things differently and our -shared
		 * linker option tells us to use the LDTLS model
		 * which assumes that there is going to be a dynamic
		 * linker. Our Popcorn binaries do not have a dynamic
		 * linker since they are of a custom static-pie variety.
		 * 
		 * Original code:
		 * return (char *)self->dtv[v[0]]+v[1]+DTP_OFFSET;
		 *
		 * which does not work. If v[0] is 0, and self->dtv[0]
		 * is 1 (indicating tls_cnt of 1), then that code is
		 * going to return an incorrect TLS block address.
		 * The TLS block address is located in dtv[1 ... N]
		 * dtv[0] always holds the 'gen' or TLS count used
		 * for deferred dtv resizing. Now it's possible that
		 * there is suppose to be a relocation to plug certain
		 * vlues into v[0] and v[1] which are entries that exist
		 * at the end of _DYNAMIC right before the GOT.
		 *
		 * For the case of Popcorn there will never be more than
		 * a single TLS block because we are using a statically
		 * linked executable. The following works.
		 */
		  return (char *)self->dtv[1]+DTP_OFFSET;
	}
	return __tls_get_new(v);
}

weak_alias(__tls_get_addr, __tls_get_new);
