## Process this file with automake to produce Makefile.in

ACLOCAL_AMFLAGS = -I . -I ./config

###############################################################################
# Popcorn note: removing the testsuite from the build process
###############################################################################
#SUBDIRS = testsuite

config_path = @config_path@
search_path = $(addprefix $(top_srcdir)/config/, $(config_path)) $(top_srcdir)

fincludedir = $(includedir)/libopenpop
libsubincludedir = $(includedir)

vpath % $(strip $(search_path))

AM_CPPFLAGS = $(addprefix -I, $(search_path))
AM_CFLAGS = $(XCFLAGS)
AM_LDFLAGS = $(XLDFLAGS) $(SECTION_LDFLAGS) $(OPT_LDFLAGS)

toolexeclib_LTLIBRARIES = libopenpop.la
nodist_toolexeclib_HEADERS = libopenpop.spec

if LIBGOMP_BUILD_VERSIONED_SHLIB
# -Wc is only a libtool option.
comma = ,
PREPROCESS = $(subst -Wc$(comma), , $(COMPILE)) -E

libopenpop.ver: $(top_srcdir)/libopenpop.map
	$(EGREP) -v '#(#| |$$)' $< | \
	  $(PREPROCESS) -P -include config.h - > $@ || (rm -f $@ ; exit 1)

if LIBGOMP_BUILD_VERSIONED_SHLIB_GNU
libopenpop_version_script = -Wl,--version-script,libopenpop.ver
libopenpop_version_dep = libopenpop.ver
endif
if LIBGOMP_BUILD_VERSIONED_SHLIB_SUN
libopenpop_version_script = -Wl,-M,libopenpop.ver-sun
libopenpop_version_dep = libopenpop.ver-sun
libopenpop.ver-sun : libopenpop.ver \
		$(top_srcdir)/../contrib/make_sunver.pl \
		$(libopenpop_la_OBJECTS) $(libopenpop_la_LIBADD)
	perl $(top_srcdir)/../contrib/make_sunver.pl \
	  libopenpop.ver \
	  $(libopenpop_la_OBJECTS:%.lo=.libs/%.o) \
	 `echo $(libopenpop_la_LIBADD) | \
	    sed 's,/\([^/.]*\)\.la,/.libs/\1.a,g'` \
	 > $@ || (rm -f $@ ; exit 1)
endif
else
libopenpop_version_script =
libopenpop_version_dep =
endif
libopenpop_version_info = -version-info $(libtool_VERSION)
libopenpop_la_LDFLAGS = $(libopenpop_version_info) $(libopenpop_version_script) \
        $(lt_host_flags)
libopenpop_la_DEPENDENCIES = $(libopenpop_version_dep)
libopenpop_la_LINK = $(LINK) $(libopenpop_la_LDFLAGS)

libopenpop_la_SOURCES = alloc.c atomic.c barrier.c critical.c env.c error.c \
	icv.c icv-device.c iter.c iter_ull.c kmp.c loop.c loop_ull.c ordered.c \
	parallel.c sections.c single.c task.c team.c work.c lock.c mutex.c \
	proc.c sem.c bar.c ptrlock.c time.c fortran.c affinity.c target.c \
	splay-tree.c libgomp-plugin.c oacc-parallel.c oacc-host.c oacc-init.c \
	oacc-mem.c oacc-async.c oacc-plugin.c oacc-cuda.c priority_queue.c

include $(top_srcdir)/plugin/Makefrag.am

if USE_FORTRAN
libopenpop_la_SOURCES += openacc.f90
endif

nodist_noinst_HEADERS = libgomp_f.h
nodist_libsubinclude_HEADERS = omp.h openacc.h
if USE_FORTRAN
nodist_finclude_HEADERS = omp_lib.h omp_lib.f90 omp_lib.mod omp_lib_kinds.mod \
	openacc_lib.h openacc.f90 openacc.mod openacc_kinds.mod
endif

LTLDFLAGS = $(shell $(SHELL) $(top_srcdir)/../libtool-ldflags $(LDFLAGS))

LINK = $(LIBTOOL) --tag CC $(AM_LIBTOOLFLAGS) $(LIBTOOLFLAGS) --mode=link \
	$(CCLD) $(AM_CFLAGS) $(CFLAGS) $(AM_LDFLAGS) $(LTLDFLAGS) -o $@

omp_lib_kinds.mod: omp_lib.mod
	:
openacc_kinds.mod: openacc.mod
	:
openacc.mod: openacc.lo
	:
%.mod: %.f90
	$(FC) $(FCFLAGS) -fsyntax-only $<
fortran.lo: libgomp_f.h
fortran.o: libgomp_f.h
env.lo: libgomp_f.h
env.o: libgomp_f.h

###############################################################################
# Popcorn note: we don't need the documentation, which is contained elsewhere
# in the upper-level GCC source directory
###############################################################################

## Automake Documentation:
## If your package has Texinfo files in many directories, you can use the
## variable TEXINFO_TEX to tell Automake where to find the canonical
## `texinfo.tex' for your package. The value of this variable should be
## the relative path from the current `Makefile.am' to `texinfo.tex'.
#TEXINFO_TEX   = ../gcc/doc/include/texinfo.tex
#
## Defines info, dvi, pdf and html targets
#MAKEINFOFLAGS = -I $(srcdir)/../gcc/doc/include
#info_TEXINFOS = libgomp.texi
#
## AM_CONDITIONAL on configure option --generated-files-in-srcdir
#if GENINSRC
#STAMP_GENINSRC = stamp-geninsrc
#else
#STAMP_GENINSRC =
#endif
#
## AM_CONDITIONAL on configure check ACX_CHECK_PROG_VER([MAKEINFO])
#if BUILD_INFO
#STAMP_BUILD_INFO = stamp-build-info
#else
#STAMP_BUILD_INFO =
#endif
#
#
#all-local: $(STAMP_GENINSRC)
#
#stamp-geninsrc: libgomp.info
#	cp -p $(top_builddir)/libgomp.info $(srcdir)/libgomp.info
#	@touch $@
#
#libgomp.info: $(STAMP_BUILD_INFO)
#
#stamp-build-info: libgomp.texi
#	$(MAKEINFO) $(AM_MAKEINFOFLAGS) $(MAKEINFOFLAGS) -I $(srcdir) -o libgomp.info $(srcdir)/libgomp.texi
#	@touch $@
#
#
#CLEANFILES = $(STAMP_GENINSRC) $(STAMP_BUILD_INFO) libgomp.info
#MAINTAINERCLEANFILES = $(srcdir)/libgomp.info
