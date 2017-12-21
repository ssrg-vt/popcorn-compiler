/*
 * Maintains highest stack address dedicated to function activations for main
 * thread.
 */
void *__popcorn_stack_base;

/*
 * C++ applications use a __dso_handle object per shared library in order to
 * correctly handle destructors for early unloading of shared-libraries (see
 * https://itanium-cxx-abi.github.io/cxx-abi/abi.html).  This is normally
 * defined statically in crtbegin, but because constructor/destructor
 * functionality is defined differently in musl, we define it here.
 */
void *__dso_handle = (void *)&__dso_handle;

