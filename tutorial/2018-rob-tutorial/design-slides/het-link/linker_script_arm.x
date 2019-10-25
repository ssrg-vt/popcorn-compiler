/* Default linker script, for normal executables */
OUTPUT_FORMAT("elf64-littleaarch64", "elf64-bigaarch64",
	      "elf64-littleaarch64")
OUTPUT_ARCH(aarch64)
ENTRY(_start)
SECTIONS
{
  /* Read-only sections, merged into text segment: */
  PROVIDE (__executable_start = SEGMENT_START("text-segment", 0x400000)); . = SEGMENT_START("text-segment", 0x400000) + SIZEOF_HEADERS;
  .interp         : { *(.interp) }
/*  .note.gnu.build-id : { *(.note.gnu.build-id) }*/
  .hash           : { *(.hash) }
  .gnu.hash       : { *(.gnu.hash) }
  .dynsym         : { *(.dynsym) }
  .dynstr         : { *(.dynstr) }
  .gnu.version    : { *(.gnu.version) }
  .gnu.version_d  : { *(.gnu.version_d) }
  .gnu.version_r  : { *(.gnu.version_r) }
  .rela.init      : { *(.rela.init) }
  .rela.text      : { *(.rela.text .rela.text.* .rela.gnu.linkonce.t.*) }
  .rela.fini      : { *(.rela.fini) }
  .rela.rodata    : { *(.rela.rodata .rela.rodata.* .rela.gnu.linkonce.r.*) }
  .rela.data.rel.ro   : { *(.rela.data.rel.ro .rela.data.rel.ro.* .rela.gnu.linkonce.d.rel.ro.*) }
  .rela.data      : { *(.rela.data .rela.data.* .rela.gnu.linkonce.d.*) }
  .rela.tdata	  : { *(.rela.tdata .rela.tdata.* .rela.gnu.linkonce.td.*) }
  .rela.tbss	  : { *(.rela.tbss .rela.tbss.* .rela.gnu.linkonce.tb.*) }
  .rela.ctors     : { *(.rela.ctors) }
  .rela.dtors     : { *(.rela.dtors) }
  .rela.got       : { *(.rela.got) }
  .rela.bss       : { *(.rela.bss .rela.bss.* .rela.gnu.linkonce.b.*) }
  .rela.ifunc     : { *(.rela.ifunc) }
  .rela.plt       :
    {
      *(.rela.plt)
      PROVIDE_HIDDEN (__rela_iplt_start = .);
      *(.rela.iplt)
      PROVIDE_HIDDEN (__rela_iplt_end = .);
    }
  .init           :
  {
    /*KEEP (*(SORT_NONE(.init)))*/
  }
  .plt            : ALIGN(16) { *(.plt) *(.iplt) }
  
.text	: ALIGN(0x100000)
{
	. = . + 1;
	. = ALIGN(0x1000);
	. = ALIGN(0x4); /* align for .text.exit */
	"/home/rlyerly/Downloads/popcorn-compiler/test-install/aarch64/lib/libc.a(exit.o)"(.text.exit); /* size 0x24 */
	. = ALIGN(0x10); /* align for .text.main */
	"main.o"(.text.main); /* size 0x7c */
	. = ALIGN(0x10); /* align for .text.fizzbuzz */
	"fizzbuzz.o"(.text.fizzbuzz); /* size 0xfc */
	. = ALIGN(0x4); /* align for .text._start_c */
	"/home/rlyerly/Downloads/popcorn-compiler/test-install/aarch64/lib/crt1.o"(.text._start_c); /* size 0x34 */
	. = ALIGN(0x4); /* align for .text.dummy */
	"/home/rlyerly/Downloads/popcorn-compiler/test-install/aarch64/lib/libc.a(__libc_start_main.o)"(.text.dummy); /* size 0x4 */
	. = ALIGN(0x4); /* align for .text.dummy1 */
	"/home/rlyerly/Downloads/popcorn-compiler/test-install/aarch64/lib/libc.a(__libc_start_main.o)"(.text.dummy1); /* size 0x4 */
	. = ALIGN(0x4); /* align for .text.__init_libc */
	"/home/rlyerly/Downloads/popcorn-compiler/test-install/aarch64/lib/libc.a(__libc_start_main.o)"(.text.__init_libc); /* size 0x1d8 */
	. = ALIGN(0x4); /* align for .text.libc_start_init */
	"/home/rlyerly/Downloads/popcorn-compiler/test-install/aarch64/lib/libc.a(__libc_start_main.o)"(.text.libc_start_init); /* size 0x40 */
	. = ALIGN(0x4); /* align for .text.__libc_start_main */
	"/home/rlyerly/Downloads/popcorn-compiler/test-install/aarch64/lib/libc.a(__libc_start_main.o)"(.text.__libc_start_main); /* size 0x50 */
	. = ALIGN(0x4); /* align for .text.libc_exit_fini */
	"/home/rlyerly/Downloads/popcorn-compiler/test-install/aarch64/lib/libc.a(exit.o)"(.text.libc_exit_fini); /* size 0x3c */
	. = ALIGN(0x4); /* align for .text.dummy */
	"/home/rlyerly/Downloads/popcorn-compiler/test-install/aarch64/lib/libc.a(exit.o)"(.text.dummy); /* size 0x4 */
	. = ALIGN(0x4); /* align for .text.printf */
	"/home/rlyerly/Downloads/popcorn-compiler/test-install/aarch64/lib/libc.a(printf.o)"(.text.printf); /* size 0x88 */
	. = . + 0x21; /* padding after .text.printf */
	. = ALIGN(0x4); /* align for .text.vfprintf */
	"/home/rlyerly/Downloads/popcorn-compiler/test-install/aarch64/lib/libc.a(vfprintf.o)"(.text.vfprintf); /* size 0x1a0 */
	. = . + 0x1a; /* padding after .text.vfprintf */
	. = ALIGN(0x4); /* align for .text.printf_core */
	"/home/rlyerly/Downloads/popcorn-compiler/test-install/aarch64/lib/libc.a(vfprintf.o)"(.text.printf_core); /* size 0x1cc4 */
	. = . + 0x142; /* padding after .text.printf_core */
	. = ALIGN(0x4); /* align for .text.getint */
	"/home/rlyerly/Downloads/popcorn-compiler/test-install/aarch64/lib/libc.a(vfprintf.o)"(.text.getint); /* size 0x74 */
	. = ALIGN(0x4); /* align for .text.pop_arg */
	"/home/rlyerly/Downloads/popcorn-compiler/test-install/aarch64/lib/libc.a(vfprintf.o)"(.text.pop_arg); /* size 0x27c */
	. = ALIGN(0x4); /* align for .text.pad */
	"/home/rlyerly/Downloads/popcorn-compiler/test-install/aarch64/lib/libc.a(vfprintf.o)"(.text.pad); /* size 0xc4 */
	. = ALIGN(0x4); /* align for .text.atoi */
	"/home/rlyerly/Downloads/popcorn-compiler/test-install/aarch64/lib/libc.a(atoi.o)"(.text.atoi); /* size 0x74 */
	. = ALIGN(0x10); /* align for .text.strnlen */
	"/home/rlyerly/Downloads/popcorn-compiler/test-install/aarch64/lib/libc.a(strnlen.o)"(.text.strnlen); /* size 0x38 */
	. = ALIGN(0x4); /* align for .text.memset */
	"/home/rlyerly/Downloads/popcorn-compiler/test-install/aarch64/lib/libc.a(memset.o)"(.text.memset); /* size 0x158 */
	. = ALIGN(0x4); /* align for .text.__init_tp */
	"/home/rlyerly/Downloads/popcorn-compiler/test-install/aarch64/lib/libc.a(__init_tls.o)"(.text.__init_tp); /* size 0x70 */
	. = ALIGN(0x4); /* align for .text.__copy_tls */
	"/home/rlyerly/Downloads/popcorn-compiler/test-install/aarch64/lib/libc.a(__init_tls.o)"(.text.__copy_tls); /* size 0x8c */
	. = ALIGN(0x4); /* align for .text.static_init_tls */
	"/home/rlyerly/Downloads/popcorn-compiler/test-install/aarch64/lib/libc.a(__init_tls.o)"(.text.static_init_tls); /* size 0x15c */
	. = ALIGN(0x4); /* align for .text.__errno_location */
	"/home/rlyerly/Downloads/popcorn-compiler/test-install/aarch64/lib/libc.a(__errno_location.o)"(.text.__errno_location); /* size 0xc */
	. = . + 0x2; /* padding after .text.__errno_location */
	. = ALIGN(0x4); /* align for .text.__strerror_l */
	"/home/rlyerly/Downloads/popcorn-compiler/test-install/aarch64/lib/libc.a(strerror.o)"(.text.__strerror_l); /* size 0x60 */
	. = ALIGN(0x4); /* align for .text.strerror */
	"/home/rlyerly/Downloads/popcorn-compiler/test-install/aarch64/lib/libc.a(strerror.o)"(.text.strerror); /* size 0xc */
	. = . + 0x9; /* padding after .text.strerror */
	. = ALIGN(0x4); /* align for .text._Exit */
	"/home/rlyerly/Downloads/popcorn-compiler/test-install/aarch64/lib/libc.a(_Exit.o)"(.text._Exit); /* size 0x20 */
	. = ALIGN(0x4); /* align for .text.dummy */
	"/home/rlyerly/Downloads/popcorn-compiler/test-install/aarch64/lib/libc.a(__lctrans.o)"(.text.dummy); /* size 0x4 */
	. = ALIGN(0x4); /* align for .text.__lctrans */
	"/home/rlyerly/Downloads/popcorn-compiler/test-install/aarch64/lib/libc.a(__lctrans.o)"(.text.__lctrans); /* size 0x4 */
	. = . + 0x1; /* padding after .text.__lctrans */
	. = ALIGN(0x4); /* align for .text.__lctrans_cur */
	"/home/rlyerly/Downloads/popcorn-compiler/test-install/aarch64/lib/libc.a(__lctrans.o)"(.text.__lctrans_cur); /* size 0x10 */
	. = . + 0x9; /* padding after .text.__lctrans_cur */
	. = ALIGN(0x4); /* align for .text.__fpclassifyl */
	"/home/rlyerly/Downloads/popcorn-compiler/test-install/aarch64/lib/libc.a(__fpclassifyl.o)"(.text.__fpclassifyl); /* size 0x54 */
	. = . + 0x1; /* padding after .text.__fpclassifyl */
	. = ALIGN(0x4); /* align for .text.__signbitl */
	"/home/rlyerly/Downloads/popcorn-compiler/test-install/aarch64/lib/libc.a(__signbitl.o)"(.text.__signbitl); /* size 0x18 */
	. = ALIGN(0x4); /* align for .text.frexpl */
	"/home/rlyerly/Downloads/popcorn-compiler/test-install/aarch64/lib/libc.a(frexpl.o)"(.text.frexpl); /* size 0xb4 */
	. = ALIGN(0x4); /* align for .text.wctomb */
	"/home/rlyerly/Downloads/popcorn-compiler/test-install/aarch64/lib/libc.a(wctomb.o)"(.text.wctomb); /* size 0x24 */
	. = ALIGN(0x4); /* align for .text.__lockfile */
	"/home/rlyerly/Downloads/popcorn-compiler/test-install/aarch64/lib/libc.a(__lockfile.o)"(.text.__lockfile); /* size 0x70 */
	. = . + 0x2; /* padding after .text.__lockfile */
	. = ALIGN(0x4); /* align for .text.__unlockfile */
	"/home/rlyerly/Downloads/popcorn-compiler/test-install/aarch64/lib/libc.a(__lockfile.o)"(.text.__unlockfile); /* size 0x4c */
	. = ALIGN(0x4); /* align for .text.dummy */
	"/home/rlyerly/Downloads/popcorn-compiler/test-install/aarch64/lib/libc.a(__stdio_close.o)"(.text.dummy); /* size 0x4 */
	. = ALIGN(0x4); /* align for .text.__stdio_close */
	"/home/rlyerly/Downloads/popcorn-compiler/test-install/aarch64/lib/libc.a(__stdio_close.o)"(.text.__stdio_close); /* size 0x28 */
	. = ALIGN(0x4); /* align for .text.__stdio_seek */
	"/home/rlyerly/Downloads/popcorn-compiler/test-install/aarch64/lib/libc.a(__stdio_seek.o)"(.text.__stdio_seek); /* size 0x14 */
	. = . + 0x2; /* padding after .text.__stdio_seek */
	. = ALIGN(0x4); /* align for .text.__stdout_write */
	"/home/rlyerly/Downloads/popcorn-compiler/test-install/aarch64/lib/libc.a(__stdout_write.o)"(.text.__stdout_write); /* size 0x68 */
	. = ALIGN(0x4); /* align for .text.__fwritex */
	"/home/rlyerly/Downloads/popcorn-compiler/test-install/aarch64/lib/libc.a(fwrite.o)"(.text.__fwritex); /* size 0xf4 */
	. = ALIGN(0x4); /* align for .text.fwrite */
	"/home/rlyerly/Downloads/popcorn-compiler/test-install/aarch64/lib/libc.a(fwrite.o)"(.text.fwrite); /* size 0x9c */
	. = ALIGN(0x10); /* align for .text.memchr */
	"/home/rlyerly/Downloads/popcorn-compiler/test-install/aarch64/lib/libc.a(memchr.o)"(.text.memchr); /* size 0xb0 */
	. = . + 0x30; /* padding after .text.memchr */
	. = ALIGN(0x4); /* align for .text.memcpy */
	"/home/rlyerly/Downloads/popcorn-compiler/test-install/aarch64/lib/libc.a(memcpy.o)"(.text.memcpy); /* size 0x3e0 */
	. = ALIGN(0x4); /* align for .text.__wait */
	"/home/rlyerly/Downloads/popcorn-compiler/test-install/aarch64/lib/libc.a(__wait.o)"(.text.__wait); /* size 0xd8 */
	. = ALIGN(0x1); /* align for .text.__set_thread_area */
	"/home/rlyerly/Downloads/popcorn-compiler/test-install/aarch64/lib/libc.a(__set_thread_area.o)"(.text.__set_thread_area); /* size 0xc */
	. = . + 0x4; /* padding after .text.__set_thread_area */
	. = ALIGN(0x10); /* align for .text.__syscall_ret */
	"/home/rlyerly/Downloads/popcorn-compiler/test-install/aarch64/lib/libc.a(syscall_ret.o)"(.text.__syscall_ret); /* size 0x30 */
	. = ALIGN(0x4); /* align for .text.wcrtomb */
	"/home/rlyerly/Downloads/popcorn-compiler/test-install/aarch64/lib/libc.a(wcrtomb.o)"(.text.wcrtomb); /* size 0x10c */
	. = . + 0xc; /* padding after .text.wcrtomb */
	. = ALIGN(0x4); /* align for .text.__stdio_write */
	"/home/rlyerly/Downloads/popcorn-compiler/test-install/aarch64/lib/libc.a(__stdio_write.o)"(.text.__stdio_write); /* size 0x138 */
	. = ALIGN(0x4); /* align for .text.__towrite */
	"/home/rlyerly/Downloads/popcorn-compiler/test-install/aarch64/lib/libc.a(__towrite.o)"(.text.__towrite); /* size 0x50 */
	. = ALIGN(0x4); /* align for .text.__towrite_needs_stdio_exit */
	"/home/rlyerly/Downloads/popcorn-compiler/test-install/aarch64/lib/libc.a(__towrite.o)"(.text.__towrite_needs_stdio_exit); /* size 0x4 */
	. = . + 0x1; /* padding after .text.__towrite_needs_stdio_exit */
	. = ALIGN(0x4); /* align for .text.__stdio_exit */
	"/home/rlyerly/Downloads/popcorn-compiler/test-install/aarch64/lib/libc.a(__stdio_exit.o)"(.text.__stdio_exit); /* size 0x48 */
	. = ALIGN(0x4); /* align for .text.close_file */
	"/home/rlyerly/Downloads/popcorn-compiler/test-install/aarch64/lib/libc.a(__stdio_exit.o)"(.text.close_file); /* size 0x70 */
	. = ALIGN(0x4); /* align for .text.__ofl_lock */
	"/home/rlyerly/Downloads/popcorn-compiler/test-install/aarch64/lib/libc.a(ofl.o)"(.text.__ofl_lock); /* size 0x24 */
	. = ALIGN(0x4); /* align for .text.__ofl_unlock */
	"/home/rlyerly/Downloads/popcorn-compiler/test-install/aarch64/lib/libc.a(ofl.o)"(.text.__ofl_unlock); /* size 0xc */
	. = ALIGN(0x4); /* align for .text.__lock */
	"/home/rlyerly/Downloads/popcorn-compiler/test-install/aarch64/lib/libc.a(__lock.o)"(.text.__lock); /* size 0x64 */
	. = ALIGN(0x4); /* align for .text.__unlock */
	"/home/rlyerly/Downloads/popcorn-compiler/test-install/aarch64/lib/libc.a(__lock.o)"(.text.__unlock); /* size 0x54 */
	. = ALIGN(0x10); /* align for .text.__init_nodes_info */
	"/home/rlyerly/Downloads/popcorn-compiler/test-install/aarch64/lib/libmigrate.a(migrate.o)"(.text.__init_nodes_info); /* size 0x7c */
	. = ALIGN(0x10); /* align for .text.check_migrate */
	"/home/rlyerly/Downloads/popcorn-compiler/test-install/aarch64/lib/libmigrate.a(migrate.o)"(.text.check_migrate); /* size 0x38 */
	. = ALIGN(0x10); /* align for .text.do_migrate */
	"/home/rlyerly/Downloads/popcorn-compiler/test-install/aarch64/lib/libmigrate.a(migrate.o)"(.text.do_migrate); /* size 0x18 */
	. = ALIGN(0x10); /* align for .text.__migrate_shim_internal */
	"/home/rlyerly/Downloads/popcorn-compiler/test-install/aarch64/lib/libmigrate.a(migrate.o)"(.text.__migrate_shim_internal); /* size 0x53c */
	. = ALIGN(0x10); /* align for .text.migrate */
	"/home/rlyerly/Downloads/popcorn-compiler/test-install/aarch64/lib/libmigrate.a(migrate.o)"(.text.migrate); /* size 0x14 */
	. = ALIGN(0x10); /* align for .text.register_migrate_callback */
	"/home/rlyerly/Downloads/popcorn-compiler/test-install/aarch64/lib/libmigrate.a(migrate.o)"(.text.register_migrate_callback); /* size 0x14 */
	. = ALIGN(0x10); /* align for .text.__cyg_profile_func_enter */
	"/home/rlyerly/Downloads/popcorn-compiler/test-install/aarch64/lib/libmigrate.a(migrate.o)"(.text.__cyg_profile_func_enter); /* size 0x2c */
	. = ALIGN(0x10); /* align for .text.__print_response_timing */
	"/home/rlyerly/Downloads/popcorn-compiler/test-install/aarch64/lib/libmigrate.a(trigger.o)"(.text.__print_response_timing); /* size 0x6c */
	. = ALIGN(0x10); /* align for .text.clear_migrate_flag */
	"/home/rlyerly/Downloads/popcorn-compiler/test-install/aarch64/lib/libmigrate.a(trigger.o)"(.text.clear_migrate_flag); /* size 0xf8 */
	. = ALIGN(0x10); /* align for .text.__register_migrate_sighandler */
	"/home/rlyerly/Downloads/popcorn-compiler/test-install/aarch64/lib/libmigrate.a(trigger.o)"(.text.__register_migrate_sighandler); /* size 0x60 */
	. = ALIGN(0x10); /* align for .text.__migrate_sighandler */
	"/home/rlyerly/Downloads/popcorn-compiler/test-install/aarch64/lib/libmigrate.a(trigger.o)"(.text.__migrate_sighandler); /* size 0x54 */
	. = ALIGN(0x10); /* align for .text.__st_userspace_ctor */
	"/home/rlyerly/Downloads/popcorn-compiler/test-install/aarch64/lib/libstack-transform.a(userspace.o)"(.text.__st_userspace_ctor); /* size 0x31c */
	. = . + 0xb4; /* padding after .text.__st_userspace_ctor */
	. = ALIGN(0x10); /* align for .text.__st_userspace_dtor */
	"/home/rlyerly/Downloads/popcorn-compiler/test-install/aarch64/lib/libstack-transform.a(userspace.o)"(.text.__st_userspace_dtor); /* size 0x98 */
	. = ALIGN(0x10); /* align for .text.get_stack_bounds */
	"/home/rlyerly/Downloads/popcorn-compiler/test-install/aarch64/lib/libstack-transform.a(userspace.o)"(.text.get_stack_bounds); /* size 0xd8 */
	. = ALIGN(0x10); /* align for .text.st_userspace_rewrite */
	"/home/rlyerly/Downloads/popcorn-compiler/test-install/aarch64/lib/libstack-transform.a(userspace.o)"(.text.st_userspace_rewrite); /* size 0x30 */
	. = ALIGN(0x10); /* align for .text.userspace_rewrite_internal */
	"/home/rlyerly/Downloads/popcorn-compiler/test-install/aarch64/lib/libstack-transform.a(userspace.o)"(.text.userspace_rewrite_internal); /* size 0x144 */
	. = . + 0x23; /* padding after .text.userspace_rewrite_internal */
	. = ALIGN(0x10); /* align for .text.st_userspace_rewrite_aarch64 */
	"/home/rlyerly/Downloads/popcorn-compiler/test-install/aarch64/lib/libstack-transform.a(userspace.o)"(.text.st_userspace_rewrite_aarch64); /* size 0x1c */
	. = ALIGN(0x10); /* align for .text.st_userspace_rewrite_powerpc64 */
	"/home/rlyerly/Downloads/popcorn-compiler/test-install/aarch64/lib/libstack-transform.a(userspace.o)"(.text.st_userspace_rewrite_powerpc64); /* size 0x1c */
	. = ALIGN(0x10); /* align for .text.st_userspace_rewrite_x86_64 */
	"/home/rlyerly/Downloads/popcorn-compiler/test-install/aarch64/lib/libstack-transform.a(userspace.o)"(.text.st_userspace_rewrite_x86_64); /* size 0x1c */
	. = ALIGN(0x10); /* align for .text.__st_ctor */
	"/home/rlyerly/Downloads/popcorn-compiler/test-install/aarch64/lib/libstack-transform.a(init.o)"(.text.__st_ctor); /* size 0x4 */
	. = . + 0x1; /* padding after .text.__st_ctor */
	. = ALIGN(0x10); /* align for .text.__st_dtor */
	"/home/rlyerly/Downloads/popcorn-compiler/test-install/aarch64/lib/libstack-transform.a(init.o)"(.text.__st_dtor); /* size 0x4 */
	. = . + 0x1; /* padding after .text.__st_dtor */
	. = ALIGN(0x10); /* align for .text.st_init */
	"/home/rlyerly/Downloads/popcorn-compiler/test-install/aarch64/lib/libstack-transform.a(init.o)"(.text.st_init); /* size 0x1e8 */
	. = . + 0x2a; /* padding after .text.st_init */
	. = ALIGN(0x10); /* align for .text.st_destroy */
	"/home/rlyerly/Downloads/popcorn-compiler/test-install/aarch64/lib/libstack-transform.a(init.o)"(.text.st_destroy); /* size 0x40 */
	. = ALIGN(0x10); /* align for .text.st_rewrite_stack */
	"/home/rlyerly/Downloads/popcorn-compiler/test-install/aarch64/lib/libstack-transform.a(rewrite.o)"(.text.st_rewrite_stack); /* size 0x7e8 */
	. = . + 0x24; /* padding after .text.st_rewrite_stack */
	. = ALIGN(0x10); /* align for .text.rewrite_frame */
	"/home/rlyerly/Downloads/popcorn-compiler/test-install/aarch64/lib/libstack-transform.a(rewrite.o)"(.text.rewrite_frame); /* size 0x3dc */
	. = . + 0x66; /* padding after .text.rewrite_frame */
	. = ALIGN(0x10); /* align for .text.st_rewrite_ondemand */
	"/home/rlyerly/Downloads/popcorn-compiler/test-install/aarch64/lib/libstack-transform.a(rewrite.o)"(.text.st_rewrite_ondemand); /* size 0x2c */
	. = ALIGN(0x10); /* align for .text.rewrite_val */
	"/home/rlyerly/Downloads/popcorn-compiler/test-install/aarch64/lib/libstack-transform.a(rewrite.o)"(.text.rewrite_val); /* size 0x298 */
	. = ALIGN(0x10); /* align for .text.first_frame */
	"/home/rlyerly/Downloads/popcorn-compiler/test-install/aarch64/lib/libstack-transform.a(unwind.o)"(.text.first_frame); /* size 0xc */
	. = ALIGN(0x10); /* align for .text.calculate_cfa */
	"/home/rlyerly/Downloads/popcorn-compiler/test-install/aarch64/lib/libstack-transform.a(unwind.o)"(.text.calculate_cfa); /* size 0x40 */
	. = ALIGN(0x10); /* align for .text.bootstrap_first_frame */
	"/home/rlyerly/Downloads/popcorn-compiler/test-install/aarch64/lib/libstack-transform.a(unwind.o)"(.text.bootstrap_first_frame); /* size 0x34 */
	. = ALIGN(0x10); /* align for .text.bootstrap_first_frame_funcentry */
	"/home/rlyerly/Downloads/popcorn-compiler/test-install/aarch64/lib/libstack-transform.a(unwind.o)"(.text.bootstrap_first_frame_funcentry); /* size 0x64 */
	. = ALIGN(0x10); /* align for .text.pop_frame */
	"/home/rlyerly/Downloads/popcorn-compiler/test-install/aarch64/lib/libstack-transform.a(unwind.o)"(.text.pop_frame); /* size 0x258 */
	. = ALIGN(0x10); /* align for .text.pop_frame_funcentry */
	"/home/rlyerly/Downloads/popcorn-compiler/test-install/aarch64/lib/libstack-transform.a(unwind.o)"(.text.pop_frame_funcentry); /* size 0x198 */
	. = ALIGN(0x10); /* align for .text.get_register_save_loc */
	"/home/rlyerly/Downloads/popcorn-compiler/test-install/aarch64/lib/libstack-transform.a(unwind.o)"(.text.get_register_save_loc); /* size 0x84 */
	. = ALIGN(0x10); /* align for .text.clear_activation */
	"/home/rlyerly/Downloads/popcorn-compiler/test-install/aarch64/lib/libstack-transform.a(unwind.o)"(.text.clear_activation); /* size 0x4c */
	. = ALIGN(0x10); /* align for .text.arch_name */
	"/home/rlyerly/Downloads/popcorn-compiler/test-install/aarch64/lib/libstack-transform.a(util.o)"(.text.arch_name); /* size 0x4c */
	. = ALIGN(0x10); /* align for .text.get_regops */
	"/home/rlyerly/Downloads/popcorn-compiler/test-install/aarch64/lib/libstack-transform.a(util.o)"(.text.get_regops); /* size 0x48 */
	. = ALIGN(0x10); /* align for .text.get_properties */
	"/home/rlyerly/Downloads/popcorn-compiler/test-install/aarch64/lib/libstack-transform.a(util.o)"(.text.get_properties); /* size 0x48 */
	. = ALIGN(0x10); /* align for .text.get_section */
	"/home/rlyerly/Downloads/popcorn-compiler/test-install/aarch64/lib/libstack-transform.a(util.o)"(.text.get_section); /* size 0xc0 */
	. = ALIGN(0x10); /* align for .text.get_num_entries */
	"/home/rlyerly/Downloads/popcorn-compiler/test-install/aarch64/lib/libstack-transform.a(util.o)"(.text.get_num_entries); /* size 0x58 */
	. = ALIGN(0x10); /* align for .text.get_section_data */
	"/home/rlyerly/Downloads/popcorn-compiler/test-install/aarch64/lib/libstack-transform.a(util.o)"(.text.get_section_data); /* size 0x3c */
	. = ALIGN(0x10); /* align for .text.get_site_by_addr */
	"/home/rlyerly/Downloads/popcorn-compiler/test-install/aarch64/lib/libstack-transform.a(util.o)"(.text.get_site_by_addr); /* size 0xa4 */
	. = ALIGN(0x10); /* align for .text.get_site_by_id */
	"/home/rlyerly/Downloads/popcorn-compiler/test-install/aarch64/lib/libstack-transform.a(util.o)"(.text.get_site_by_id); /* size 0x90 */
	. = ALIGN(0x10); /* align for .text.get_unwind_offset_by_addr */
	"/home/rlyerly/Downloads/popcorn-compiler/test-install/aarch64/lib/libstack-transform.a(util.o)"(.text.get_unwind_offset_by_addr); /* size 0xb4 */
	. = ALIGN(0x10); /* align for .text.get_function_address */
	"/home/rlyerly/Downloads/popcorn-compiler/test-install/aarch64/lib/libstack-transform.a(util.o)"(.text.get_function_address); /* size 0x88 */
	. = ALIGN(0x10); /* align for .text.align_sp_aarch64 */
	"/home/rlyerly/Downloads/popcorn-compiler/test-install/aarch64/lib/libstack-transform.a(properties.o)"(.text.align_sp_aarch64); /* size 0xc */
	. = ALIGN(0x10); /* align for .text.is_callee_saved_aarch64 */
	"/home/rlyerly/Downloads/popcorn-compiler/test-install/aarch64/lib/libstack-transform.a(properties.o)"(.text.is_callee_saved_aarch64); /* size 0x30 */
	. = ALIGN(0x10); /* align for .text.callee_reg_size_aarch64 */
	"/home/rlyerly/Downloads/popcorn-compiler/test-install/aarch64/lib/libstack-transform.a(properties.o)"(.text.callee_reg_size_aarch64); /* size 0x58 */
	. = ALIGN(0x10); /* align for .text.regset_default_aarch64 */
	"/home/rlyerly/Downloads/popcorn-compiler/test-install/aarch64/lib/libstack-transform.a(regs.o)"(.text.regset_default_aarch64); /* size 0x2c */
	. = ALIGN(0x10); /* align for .text.regset_init_aarch64 */
	"/home/rlyerly/Downloads/popcorn-compiler/test-install/aarch64/lib/libstack-transform.a(regs.o)"(.text.regset_init_aarch64); /* size 0x40 */
	. = ALIGN(0x10); /* align for .text.regset_free_aarch64 */
	"/home/rlyerly/Downloads/popcorn-compiler/test-install/aarch64/lib/libstack-transform.a(regs.o)"(.text.regset_free_aarch64); /* size 0x4 */
	. = . + 0x1; /* padding after .text.regset_free_aarch64 */
	. = ALIGN(0x10); /* align for .text.regset_clone_aarch64 */
	"/home/rlyerly/Downloads/popcorn-compiler/test-install/aarch64/lib/libstack-transform.a(regs.o)"(.text.regset_clone_aarch64); /* size 0x14 */
	. = ALIGN(0x10); /* align for .text.regset_copyin_aarch64 */
	"/home/rlyerly/Downloads/popcorn-compiler/test-install/aarch64/lib/libstack-transform.a(regs.o)"(.text.regset_copyin_aarch64); /* size 0x10 */
	. = . + 0x1; /* padding after .text.regset_copyin_aarch64 */
	. = ALIGN(0x10); /* align for .text.regset_copyout_aarch64 */
	"/home/rlyerly/Downloads/popcorn-compiler/test-install/aarch64/lib/libstack-transform.a(regs.o)"(.text.regset_copyout_aarch64); /* size 0x14 */
	. = ALIGN(0x10); /* align for .text.pc_aarch64 */
	"/home/rlyerly/Downloads/popcorn-compiler/test-install/aarch64/lib/libstack-transform.a(regs.o)"(.text.pc_aarch64); /* size 0x8 */
	. = ALIGN(0x10); /* align for .text.sp_aarch64 */
	"/home/rlyerly/Downloads/popcorn-compiler/test-install/aarch64/lib/libstack-transform.a(regs.o)"(.text.sp_aarch64); /* size 0x8 */
	. = ALIGN(0x10); /* align for .text.fbp_aarch64 */
	"/home/rlyerly/Downloads/popcorn-compiler/test-install/aarch64/lib/libstack-transform.a(regs.o)"(.text.fbp_aarch64); /* size 0x8 */
	. = ALIGN(0x10); /* align for .text.ra_reg_aarch64 */
	"/home/rlyerly/Downloads/popcorn-compiler/test-install/aarch64/lib/libstack-transform.a(regs.o)"(.text.ra_reg_aarch64); /* size 0x8 */
	. = ALIGN(0x10); /* align for .text.set_pc_aarch64 */
	"/home/rlyerly/Downloads/popcorn-compiler/test-install/aarch64/lib/libstack-transform.a(regs.o)"(.text.set_pc_aarch64); /* size 0x8 */
	. = ALIGN(0x10); /* align for .text.set_sp_aarch64 */
	"/home/rlyerly/Downloads/popcorn-compiler/test-install/aarch64/lib/libstack-transform.a(regs.o)"(.text.set_sp_aarch64); /* size 0x8 */
	. = ALIGN(0x10); /* align for .text.set_fbp_aarch64 */
	"/home/rlyerly/Downloads/popcorn-compiler/test-install/aarch64/lib/libstack-transform.a(regs.o)"(.text.set_fbp_aarch64); /* size 0x8 */
	. = ALIGN(0x10); /* align for .text.set_ra_reg_aarch64 */
	"/home/rlyerly/Downloads/popcorn-compiler/test-install/aarch64/lib/libstack-transform.a(regs.o)"(.text.set_ra_reg_aarch64); /* size 0x8 */
	. = ALIGN(0x10); /* align for .text.setup_fbp_aarch64 */
	"/home/rlyerly/Downloads/popcorn-compiler/test-install/aarch64/lib/libstack-transform.a(regs.o)"(.text.setup_fbp_aarch64); /* size 0xc */
	. = ALIGN(0x10); /* align for .text.reg_size_aarch64 */
	"/home/rlyerly/Downloads/popcorn-compiler/test-install/aarch64/lib/libstack-transform.a(regs.o)"(.text.reg_size_aarch64); /* size 0x54 */
	. = ALIGN(0x10); /* align for .text.reg_aarch64 */
	"/home/rlyerly/Downloads/popcorn-compiler/test-install/aarch64/lib/libstack-transform.a(regs.o)"(.text.reg_aarch64); /* size 0x344 */
	. = ALIGN(0x10); /* align for .text.align_sp_powerpc64 */
	"/home/rlyerly/Downloads/popcorn-compiler/test-install/aarch64/lib/libstack-transform.a(properties.o)"(.text.align_sp_powerpc64); /* size 0x14 */
	. = ALIGN(0x10); /* align for .text.is_callee_saved_powerpc64 */
	"/home/rlyerly/Downloads/popcorn-compiler/test-install/aarch64/lib/libstack-transform.a(properties.o)"(.text.is_callee_saved_powerpc64); /* size 0x2c */
	. = ALIGN(0x10); /* align for .text.callee_reg_size_powerpc64 */
	"/home/rlyerly/Downloads/popcorn-compiler/test-install/aarch64/lib/libstack-transform.a(properties.o)"(.text.callee_reg_size_powerpc64); /* size 0x50 */
	. = ALIGN(0x10); /* align for .text.regset_default_powerpc64 */
	"/home/rlyerly/Downloads/popcorn-compiler/test-install/aarch64/lib/libstack-transform.a(regs.o)"(.text.regset_default_powerpc64); /* size 0x2c */
	. = ALIGN(0x10); /* align for .text.regset_init_powerpc64 */
	"/home/rlyerly/Downloads/popcorn-compiler/test-install/aarch64/lib/libstack-transform.a(regs.o)"(.text.regset_init_powerpc64); /* size 0x40 */
	. = ALIGN(0x10); /* align for .text.regset_free_powerpc64 */
	"/home/rlyerly/Downloads/popcorn-compiler/test-install/aarch64/lib/libstack-transform.a(regs.o)"(.text.regset_free_powerpc64); /* size 0x4 */
	. = . + 0x1; /* padding after .text.regset_free_powerpc64 */
	. = ALIGN(0x10); /* align for .text.regset_clone_powerpc64 */
	"/home/rlyerly/Downloads/popcorn-compiler/test-install/aarch64/lib/libstack-transform.a(regs.o)"(.text.regset_clone_powerpc64); /* size 0x14 */
	. = ALIGN(0x10); /* align for .text.regset_copyin_powerpc64 */
	"/home/rlyerly/Downloads/popcorn-compiler/test-install/aarch64/lib/libstack-transform.a(regs.o)"(.text.regset_copyin_powerpc64); /* size 0x10 */
	. = . + 0x1; /* padding after .text.regset_copyin_powerpc64 */
	. = ALIGN(0x10); /* align for .text.regset_copyout_powerpc64 */
	"/home/rlyerly/Downloads/popcorn-compiler/test-install/aarch64/lib/libstack-transform.a(regs.o)"(.text.regset_copyout_powerpc64); /* size 0x14 */
	. = ALIGN(0x10); /* align for .text.pc_powerpc64 */
	"/home/rlyerly/Downloads/popcorn-compiler/test-install/aarch64/lib/libstack-transform.a(regs.o)"(.text.pc_powerpc64); /* size 0x8 */
	. = ALIGN(0x10); /* align for .text.sp_powerpc64 */
	"/home/rlyerly/Downloads/popcorn-compiler/test-install/aarch64/lib/libstack-transform.a(regs.o)"(.text.sp_powerpc64); /* size 0x8 */
	. = ALIGN(0x10); /* align for .text.fbp_powerpc64 */
	"/home/rlyerly/Downloads/popcorn-compiler/test-install/aarch64/lib/libstack-transform.a(regs.o)"(.text.fbp_powerpc64); /* size 0x8 */
	. = ALIGN(0x10); /* align for .text.ra_reg_powerpc64 */
	"/home/rlyerly/Downloads/popcorn-compiler/test-install/aarch64/lib/libstack-transform.a(regs.o)"(.text.ra_reg_powerpc64); /* size 0x8 */
	. = ALIGN(0x10); /* align for .text.set_pc_powerpc64 */
	"/home/rlyerly/Downloads/popcorn-compiler/test-install/aarch64/lib/libstack-transform.a(regs.o)"(.text.set_pc_powerpc64); /* size 0x8 */
	. = ALIGN(0x10); /* align for .text.set_sp_powerpc64 */
	"/home/rlyerly/Downloads/popcorn-compiler/test-install/aarch64/lib/libstack-transform.a(regs.o)"(.text.set_sp_powerpc64); /* size 0x8 */
	. = ALIGN(0x10); /* align for .text.set_fbp_powerpc64 */
	"/home/rlyerly/Downloads/popcorn-compiler/test-install/aarch64/lib/libstack-transform.a(regs.o)"(.text.set_fbp_powerpc64); /* size 0x8 */
	. = ALIGN(0x10); /* align for .text.set_ra_reg_powerpc64 */
	"/home/rlyerly/Downloads/popcorn-compiler/test-install/aarch64/lib/libstack-transform.a(regs.o)"(.text.set_ra_reg_powerpc64); /* size 0x8 */
	. = ALIGN(0x10); /* align for .text.setup_fbp_powerpc64 */
	"/home/rlyerly/Downloads/popcorn-compiler/test-install/aarch64/lib/libstack-transform.a(regs.o)"(.text.setup_fbp_powerpc64); /* size 0xc */
	. = ALIGN(0x10); /* align for .text.reg_size_powerpc64 */
	"/home/rlyerly/Downloads/popcorn-compiler/test-install/aarch64/lib/libstack-transform.a(regs.o)"(.text.reg_size_powerpc64); /* size 0x54 */
	. = ALIGN(0x10); /* align for .text.reg_powerpc64 */
	"/home/rlyerly/Downloads/popcorn-compiler/test-install/aarch64/lib/libstack-transform.a(regs.o)"(.text.reg_powerpc64); /* size 0x35c */
	. = ALIGN(0x10); /* align for .text.align_sp_x86_64 */
	"/home/rlyerly/Downloads/popcorn-compiler/test-install/aarch64/lib/libstack-transform.a(properties.o)"(.text.align_sp_x86_64); /* size 0x10 */
	. = ALIGN(0x10); /* align for .text.is_callee_saved_x86_64 */
	"/home/rlyerly/Downloads/popcorn-compiler/test-install/aarch64/lib/libstack-transform.a(properties.o)"(.text.is_callee_saved_x86_64); /* size 0x2c */
	. = ALIGN(0x10); /* align for .text.callee_reg_size_x86_64 */
	"/home/rlyerly/Downloads/popcorn-compiler/test-install/aarch64/lib/libstack-transform.a(properties.o)"(.text.callee_reg_size_x86_64); /* size 0x54 */
	. = ALIGN(0x10); /* align for .text.regset_default_x86_64 */
	"/home/rlyerly/Downloads/popcorn-compiler/test-install/aarch64/lib/libstack-transform.a(regs.o)"(.text.regset_default_x86_64); /* size 0x2c */
	. = ALIGN(0x10); /* align for .text.regset_init_x86_64 */
	"/home/rlyerly/Downloads/popcorn-compiler/test-install/aarch64/lib/libstack-transform.a(regs.o)"(.text.regset_init_x86_64); /* size 0x40 */
	. = ALIGN(0x10); /* align for .text.regset_free_x86_64 */
	"/home/rlyerly/Downloads/popcorn-compiler/test-install/aarch64/lib/libstack-transform.a(regs.o)"(.text.regset_free_x86_64); /* size 0x4 */
	. = . + 0x1; /* padding after .text.regset_free_x86_64 */
	. = ALIGN(0x10); /* align for .text.regset_clone_x86_64 */
	"/home/rlyerly/Downloads/popcorn-compiler/test-install/aarch64/lib/libstack-transform.a(regs.o)"(.text.regset_clone_x86_64); /* size 0x14 */
	. = ALIGN(0x10); /* align for .text.regset_copyin_x86_64 */
	"/home/rlyerly/Downloads/popcorn-compiler/test-install/aarch64/lib/libstack-transform.a(regs.o)"(.text.regset_copyin_x86_64); /* size 0x10 */
	. = . + 0x1; /* padding after .text.regset_copyin_x86_64 */
	. = ALIGN(0x10); /* align for .text.regset_copyout_x86_64 */
	"/home/rlyerly/Downloads/popcorn-compiler/test-install/aarch64/lib/libstack-transform.a(regs.o)"(.text.regset_copyout_x86_64); /* size 0x14 */
	. = ALIGN(0x10); /* align for .text.pc_x86_64 */
	"/home/rlyerly/Downloads/popcorn-compiler/test-install/aarch64/lib/libstack-transform.a(regs.o)"(.text.pc_x86_64); /* size 0x8 */
	. = ALIGN(0x10); /* align for .text.sp_x86_64 */
	"/home/rlyerly/Downloads/popcorn-compiler/test-install/aarch64/lib/libstack-transform.a(regs.o)"(.text.sp_x86_64); /* size 0x8 */
	. = ALIGN(0x10); /* align for .text.fbp_x86_64 */
	"/home/rlyerly/Downloads/popcorn-compiler/test-install/aarch64/lib/libstack-transform.a(regs.o)"(.text.fbp_x86_64); /* size 0x8 */
	. = ALIGN(0x10); /* align for .text.ra_reg_x86_64 */
	"/home/rlyerly/Downloads/popcorn-compiler/test-install/aarch64/lib/libstack-transform.a(regs.o)"(.text.ra_reg_x86_64); /* size 0x2c */
	. = ALIGN(0x10); /* align for .text.set_pc_x86_64 */
	"/home/rlyerly/Downloads/popcorn-compiler/test-install/aarch64/lib/libstack-transform.a(regs.o)"(.text.set_pc_x86_64); /* size 0x8 */
	. = ALIGN(0x10); /* align for .text.set_sp_x86_64 */
	"/home/rlyerly/Downloads/popcorn-compiler/test-install/aarch64/lib/libstack-transform.a(regs.o)"(.text.set_sp_x86_64); /* size 0x8 */
	. = ALIGN(0x10); /* align for .text.set_fbp_x86_64 */
	"/home/rlyerly/Downloads/popcorn-compiler/test-install/aarch64/lib/libstack-transform.a(regs.o)"(.text.set_fbp_x86_64); /* size 0x8 */
	. = ALIGN(0x10); /* align for .text.set_ra_reg_x86_64 */
	"/home/rlyerly/Downloads/popcorn-compiler/test-install/aarch64/lib/libstack-transform.a(regs.o)"(.text.set_ra_reg_x86_64); /* size 0x2c */
	. = ALIGN(0x10); /* align for .text.setup_fbp_x86_64 */
	"/home/rlyerly/Downloads/popcorn-compiler/test-install/aarch64/lib/libstack-transform.a(regs.o)"(.text.setup_fbp_x86_64); /* size 0xc */
	. = ALIGN(0x10); /* align for .text.reg_size_x86_64 */
	"/home/rlyerly/Downloads/popcorn-compiler/test-install/aarch64/lib/libstack-transform.a(regs.o)"(.text.reg_size_x86_64); /* size 0x50 */
	. = ALIGN(0x10); /* align for .text.reg_x86_64 */
	"/home/rlyerly/Downloads/popcorn-compiler/test-install/aarch64/lib/libstack-transform.a(regs.o)"(.text.reg_x86_64); /* size 0x1d0 */
	. = ALIGN(0x10); /* align for .text.put_val */
	"/home/rlyerly/Downloads/popcorn-compiler/test-install/aarch64/lib/libstack-transform.a(data.o)"(.text.put_val); /* size 0x254 */
	. = ALIGN(0x10); /* align for .text.get_dest_loc */
	"/home/rlyerly/Downloads/popcorn-compiler/test-install/aarch64/lib/libstack-transform.a(data.o)"(.text.get_dest_loc); /* size 0xdc */
	. = ALIGN(0x10); /* align for .text.put_val_arch */
	"/home/rlyerly/Downloads/popcorn-compiler/test-install/aarch64/lib/libstack-transform.a(data.o)"(.text.put_val_arch); /* size 0x3f0 */
	. = ALIGN(0x10); /* align for .text.put_val_data */
	"/home/rlyerly/Downloads/popcorn-compiler/test-install/aarch64/lib/libstack-transform.a(data.o)"(.text.put_val_data); /* size 0x114 */
	. = ALIGN(0x10); /* align for .text.points_to_stack */
	"/home/rlyerly/Downloads/popcorn-compiler/test-install/aarch64/lib/libstack-transform.a(data.o)"(.text.points_to_stack); /* size 0x138 */
	. = ALIGN(0x10); /* align for .text.points_to_data */
	"/home/rlyerly/Downloads/popcorn-compiler/test-install/aarch64/lib/libstack-transform.a(data.o)"(.text.points_to_data); /* size 0x190 */
	. = ALIGN(0x10); /* align for .text.set_return_address */
	"/home/rlyerly/Downloads/popcorn-compiler/test-install/aarch64/lib/libstack-transform.a(data.o)"(.text.set_return_address); /* size 0x24 */
	. = ALIGN(0x10); /* align for .text.set_return_address_funcentry */
	"/home/rlyerly/Downloads/popcorn-compiler/test-install/aarch64/lib/libstack-transform.a(data.o)"(.text.set_return_address_funcentry); /* size 0x44 */
	. = ALIGN(0x10); /* align for .text.get_savedfbp_loc */
	"/home/rlyerly/Downloads/popcorn-compiler/test-install/aarch64/lib/libstack-transform.a(data.o)"(.text.get_savedfbp_loc); /* size 0x90 */
	. = ALIGN(0x10); /* align for .text._elf_check_type.part.0 */
	"/home/rlyerly/Downloads/popcorn-compiler/test-install/aarch64/lib/libelf.a(begin.o)"(.text._elf_check_type.part.0); /* size 0x250 */
	. = ALIGN(0x10); /* align for .text.elf_begin */
	"/home/rlyerly/Downloads/popcorn-compiler/test-install/aarch64/lib/libelf.a(begin.o)"(.text.elf_begin); /* size 0x14fc */
	. = ALIGN(0x10); /* align for .text.elf_memory */
	"/home/rlyerly/Downloads/popcorn-compiler/test-install/aarch64/lib/libelf.a(begin.o)"(.text.elf_memory); /* size 0x164 */
	. = ALIGN(0x10); /* align for .text.gelf_getclass */
	"/home/rlyerly/Downloads/popcorn-compiler/test-install/aarch64/lib/libelf.a(begin.o)"(.text.gelf_getclass); /* size 0x40 */
	. = ALIGN(0x10); /* align for .text.elf_end */
	"/home/rlyerly/Downloads/popcorn-compiler/test-install/aarch64/lib/libelf.a(end.o)"(.text.elf_end); /* size 0x1e8 */
	. = . + 0x25; /* padding after .text.elf_end */
	. = ALIGN(0x10); /* align for .text.elf_getdata */
	"/home/rlyerly/Downloads/popcorn-compiler/test-install/aarch64/lib/libelf.a(getdata.o)"(.text.elf_getdata); /* size 0x310 */
	. = . + 0x18; /* padding after .text.elf_getdata */
	. = ALIGN(0x10); /* align for .text.elf_getident */
	"/home/rlyerly/Downloads/popcorn-compiler/test-install/aarch64/lib/libelf.a(getident.o)"(.text.elf_getident); /* size 0xc0 */
	. = ALIGN(0x10); /* align for .text.elf_nextscn */
	"/home/rlyerly/Downloads/popcorn-compiler/test-install/aarch64/lib/libelf.a(nextscn.o)"(.text.elf_nextscn); /* size 0xd0 */
	. = ALIGN(0x10); /* align for .text.elf_strptr */
	"/home/rlyerly/Downloads/popcorn-compiler/test-install/aarch64/lib/libelf.a(strptr.o)"(.text.elf_strptr); /* size 0x1b4 */
	. = ALIGN(0x10); /* align for .text.elf_version */
	"/home/rlyerly/Downloads/popcorn-compiler/test-install/aarch64/lib/libelf.a(version.o)"(.text.elf_version); /* size 0x80 */
	. = ALIGN(0x10); /* align for .text._elf_getehdr */
	"/home/rlyerly/Downloads/popcorn-compiler/test-install/aarch64/lib/libelf.a(32.getehdr.o)"(.text._elf_getehdr); /* size 0xa0 */
	. = ALIGN(0x10); /* align for .text.elf32_getehdr */
	"/home/rlyerly/Downloads/popcorn-compiler/test-install/aarch64/lib/libelf.a(32.getehdr.o)"(.text.elf32_getehdr); /* size 0xa0 */
	. = ALIGN(0x10); /* align for .text.elf64_getehdr */
	"/home/rlyerly/Downloads/popcorn-compiler/test-install/aarch64/lib/libelf.a(32.getehdr.o)"(.text.elf64_getehdr); /* size 0xa0 */
	. = ALIGN(0x10); /* align for .text.half_32L__tom */
	"/home/rlyerly/Downloads/popcorn-compiler/test-install/aarch64/lib/libelf.a(32.xlatetof.o)"(.text.half_32L__tom); /* size 0x120 */
	. = ALIGN(0x10); /* align for .text.half_32L__tof */
	"/home/rlyerly/Downloads/popcorn-compiler/test-install/aarch64/lib/libelf.a(32.xlatetof.o)"(.text.half_32L__tof); /* size 0x29c */
	. = ALIGN(0x10); /* align for .text.half_32M__tom */
	"/home/rlyerly/Downloads/popcorn-compiler/test-install/aarch64/lib/libelf.a(32.xlatetof.o)"(.text.half_32M__tom); /* size 0x14c */
	. = . + 0x6; /* padding after .text.half_32M__tom */
	. = ALIGN(0x10); /* align for .text.half_32M__tof */
	"/home/rlyerly/Downloads/popcorn-compiler/test-install/aarch64/lib/libelf.a(32.xlatetof.o)"(.text.half_32M__tof); /* size 0x2a0 */
	. = ALIGN(0x10); /* align for .text.sword_32L__tom */
	"/home/rlyerly/Downloads/popcorn-compiler/test-install/aarch64/lib/libelf.a(32.xlatetof.o)"(.text.sword_32L__tom); /* size 0xd0 */
	. = . + 0x10; /* padding after .text.sword_32L__tom */
	. = ALIGN(0x10); /* align for .text.sword_32L__tof */
	"/home/rlyerly/Downloads/popcorn-compiler/test-install/aarch64/lib/libelf.a(32.xlatetof.o)"(.text.sword_32L__tof); /* size 0x464 */
	. = . + 0xd6; /* padding after .text.sword_32L__tof */
	. = ALIGN(0x10); /* align for .text.sword_32M__tom */
	"/home/rlyerly/Downloads/popcorn-compiler/test-install/aarch64/lib/libelf.a(32.xlatetof.o)"(.text.sword_32M__tom); /* size 0xe4 */
	. = ALIGN(0x10); /* align for .text.sword_32M__tof */
	"/home/rlyerly/Downloads/popcorn-compiler/test-install/aarch64/lib/libelf.a(32.xlatetof.o)"(.text.sword_32M__tof); /* size 0x468 */
	. = . + 0xd2; /* padding after .text.sword_32M__tof */
	. = ALIGN(0x10); /* align for .text.word_32L__tom */
	"/home/rlyerly/Downloads/popcorn-compiler/test-install/aarch64/lib/libelf.a(32.xlatetof.o)"(.text.word_32L__tom); /* size 0xd0 */
	. = . + 0x10; /* padding after .text.word_32L__tom */
	. = ALIGN(0x10); /* align for .text.word_32L__tof */
	"/home/rlyerly/Downloads/popcorn-compiler/test-install/aarch64/lib/libelf.a(32.xlatetof.o)"(.text.word_32L__tof); /* size 0x464 */
	. = . + 0xd6; /* padding after .text.word_32L__tof */
	. = ALIGN(0x10); /* align for .text.word_32M__tom */
	"/home/rlyerly/Downloads/popcorn-compiler/test-install/aarch64/lib/libelf.a(32.xlatetof.o)"(.text.word_32M__tom); /* size 0xe4 */
	. = ALIGN(0x10); /* align for .text.word_32M__tof */
	"/home/rlyerly/Downloads/popcorn-compiler/test-install/aarch64/lib/libelf.a(32.xlatetof.o)"(.text.word_32M__tof); /* size 0x468 */
	. = . + 0xd2; /* padding after .text.word_32M__tof */
	. = ALIGN(0x10); /* align for .text.dyn_32L11_tom */
	"/home/rlyerly/Downloads/popcorn-compiler/test-install/aarch64/lib/libelf.a(32.xlatetof.o)"(.text.dyn_32L11_tom); /* size 0x44 */
	. = ALIGN(0x10); /* align for .text.dyn_32L11_tof */
	"/home/rlyerly/Downloads/popcorn-compiler/test-install/aarch64/lib/libelf.a(32.xlatetof.o)"(.text.dyn_32L11_tof); /* size 0x374 */
	. = . + 0x2b3; /* padding after .text.dyn_32L11_tof */
	. = ALIGN(0x10); /* align for .text.dyn_32M11_tom */
	"/home/rlyerly/Downloads/popcorn-compiler/test-install/aarch64/lib/libelf.a(32.xlatetof.o)"(.text.dyn_32M11_tom); /* size 0x4c */
	. = ALIGN(0x10); /* align for .text.dyn_32M11_tof */
	"/home/rlyerly/Downloads/popcorn-compiler/test-install/aarch64/lib/libelf.a(32.xlatetof.o)"(.text.dyn_32M11_tof); /* size 0x374 */
	. = . + 0x2b3; /* padding after .text.dyn_32M11_tof */
	. = ALIGN(0x10); /* align for .text.phdr_32L11_tom */
	"/home/rlyerly/Downloads/popcorn-compiler/test-install/aarch64/lib/libelf.a(32.xlatetof.o)"(.text.phdr_32L11_tom); /* size 0xc0 */
	. = . + 0xd; /* padding after .text.phdr_32L11_tom */
	. = ALIGN(0x10); /* align for .text.phdr_32L11_tof */
	"/home/rlyerly/Downloads/popcorn-compiler/test-install/aarch64/lib/libelf.a(32.xlatetof.o)"(.text.phdr_32L11_tof); /* size 0x154 */
	. = ALIGN(0x10); /* align for .text.phdr_32M11_tom */
	"/home/rlyerly/Downloads/popcorn-compiler/test-install/aarch64/lib/libelf.a(32.xlatetof.o)"(.text.phdr_32M11_tom); /* size 0xe0 */
	. = ALIGN(0x10); /* align for .text.phdr_32M11_tof */
	"/home/rlyerly/Downloads/popcorn-compiler/test-install/aarch64/lib/libelf.a(32.xlatetof.o)"(.text.phdr_32M11_tof); /* size 0x154 */
	. = ALIGN(0x10); /* align for .text.rela_32L11_tom */
	"/home/rlyerly/Downloads/popcorn-compiler/test-install/aarch64/lib/libelf.a(32.xlatetof.o)"(.text.rela_32L11_tom); /* size 0x5c */
	. = ALIGN(0x10); /* align for .text.rela_32L11_tof */
	"/home/rlyerly/Downloads/popcorn-compiler/test-install/aarch64/lib/libelf.a(32.xlatetof.o)"(.text.rela_32L11_tof); /* size 0xb0 */
	. = ALIGN(0x10); /* align for .text.rela_32M11_tom */
	"/home/rlyerly/Downloads/popcorn-compiler/test-install/aarch64/lib/libelf.a(32.xlatetof.o)"(.text.rela_32M11_tom); /* size 0x68 */
	. = ALIGN(0x10); /* align for .text.rela_32M11_tof */
	"/home/rlyerly/Downloads/popcorn-compiler/test-install/aarch64/lib/libelf.a(32.xlatetof.o)"(.text.rela_32M11_tof); /* size 0xb0 */
	. = ALIGN(0x10); /* align for .text.rel_32L11_tom */
	"/home/rlyerly/Downloads/popcorn-compiler/test-install/aarch64/lib/libelf.a(32.xlatetof.o)"(.text.rel_32L11_tom); /* size 0xc0 */
	. = ALIGN(0x10); /* align for .text.rel_32L11_tof */
	"/home/rlyerly/Downloads/popcorn-compiler/test-install/aarch64/lib/libelf.a(32.xlatetof.o)"(.text.rel_32L11_tof); /* size 0x354 */
	. = . + 0x303; /* padding after .text.rel_32L11_tof */
	. = ALIGN(0x10); /* align for .text.rel_32M11_tom */
	"/home/rlyerly/Downloads/popcorn-compiler/test-install/aarch64/lib/libelf.a(32.xlatetof.o)"(.text.rel_32M11_tom); /* size 0xd0 */
	. = ALIGN(0x10); /* align for .text.rel_32M11_tof */
	"/home/rlyerly/Downloads/popcorn-compiler/test-install/aarch64/lib/libelf.a(32.xlatetof.o)"(.text.rel_32M11_tof); /* size 0x354 */
	. = . + 0x303; /* padding after .text.rel_32M11_tof */
	. = ALIGN(0x10); /* align for .text.shdr_32L11_tom */
	"/home/rlyerly/Downloads/popcorn-compiler/test-install/aarch64/lib/libelf.a(32.xlatetof.o)"(.text.shdr_32L11_tom); /* size 0x17c */
	. = ALIGN(0x10); /* align for .text.shdr_32L11_tof */
	"/home/rlyerly/Downloads/popcorn-compiler/test-install/aarch64/lib/libelf.a(32.xlatetof.o)"(.text.shdr_32L11_tof); /* size 0x1ac */
	. = ALIGN(0x10); /* align for .text.shdr_32M11_tom */
	"/home/rlyerly/Downloads/popcorn-compiler/test-install/aarch64/lib/libelf.a(32.xlatetof.o)"(.text.shdr_32M11_tom); /* size 0x1dc */
	. = ALIGN(0x10); /* align for .text.shdr_32M11_tof */
	"/home/rlyerly/Downloads/popcorn-compiler/test-install/aarch64/lib/libelf.a(32.xlatetof.o)"(.text.shdr_32M11_tof); /* size 0x1ac */
	. = ALIGN(0x10); /* align for .text.sym_32L11_tom */
	"/home/rlyerly/Downloads/popcorn-compiler/test-install/aarch64/lib/libelf.a(32.xlatetof.o)"(.text.sym_32L11_tom); /* size 0x64 */
	. = ALIGN(0x10); /* align for .text.sym_32L11_tof */
	"/home/rlyerly/Downloads/popcorn-compiler/test-install/aarch64/lib/libelf.a(32.xlatetof.o)"(.text.sym_32L11_tof); /* size 0xc0 */
	. = ALIGN(0x10); /* align for .text.sym_32M11_tom */
	"/home/rlyerly/Downloads/popcorn-compiler/test-install/aarch64/lib/libelf.a(32.xlatetof.o)"(.text.sym_32M11_tom); /* size 0x74 */
	. = ALIGN(0x10); /* align for .text.sym_32M11_tof */
	"/home/rlyerly/Downloads/popcorn-compiler/test-install/aarch64/lib/libelf.a(32.xlatetof.o)"(.text.sym_32M11_tof); /* size 0xc0 */
	. = ALIGN(0x10); /* align for .text.ehdr_32M11_tom */
	"/home/rlyerly/Downloads/popcorn-compiler/test-install/aarch64/lib/libelf.a(32.xlatetof.o)"(.text.ehdr_32M11_tom); /* size 0x100 */
	. = ALIGN(0x10); /* align for .text.byte_copy */
	"/home/rlyerly/Downloads/popcorn-compiler/test-install/aarch64/lib/libelf.a(32.xlatetof.o)"(.text.byte_copy); /* size 0x34 */
	. = ALIGN(0x10); /* align for .text.ehdr_32M11_tof */
	"/home/rlyerly/Downloads/popcorn-compiler/test-install/aarch64/lib/libelf.a(32.xlatetof.o)"(.text.ehdr_32M11_tof); /* size 0x198 */
	. = ALIGN(0x10); /* align for .text.ehdr_32L11_tom */
	"/home/rlyerly/Downloads/popcorn-compiler/test-install/aarch64/lib/libelf.a(32.xlatetof.o)"(.text.ehdr_32L11_tom); /* size 0xcc */
	. = ALIGN(0x10); /* align for .text.ehdr_32L11_tof */
	"/home/rlyerly/Downloads/popcorn-compiler/test-install/aarch64/lib/libelf.a(32.xlatetof.o)"(.text.ehdr_32L11_tof); /* size 0x198 */
	. = ALIGN(0x10); /* align for .text.addr_32L__tom */
	"/home/rlyerly/Downloads/popcorn-compiler/test-install/aarch64/lib/libelf.a(32.xlatetof.o)"(.text.addr_32L__tom); /* size 0xd0 */
	. = . + 0x10; /* padding after .text.addr_32L__tom */
	. = ALIGN(0x10); /* align for .text.addr_32L__tof */
	"/home/rlyerly/Downloads/popcorn-compiler/test-install/aarch64/lib/libelf.a(32.xlatetof.o)"(.text.addr_32L__tof); /* size 0x464 */
	. = . + 0xd6; /* padding after .text.addr_32L__tof */
	. = ALIGN(0x10); /* align for .text.addr_32M__tom */
	"/home/rlyerly/Downloads/popcorn-compiler/test-install/aarch64/lib/libelf.a(32.xlatetof.o)"(.text.addr_32M__tom); /* size 0xe4 */
	. = ALIGN(0x10); /* align for .text.addr_32M__tof */
	"/home/rlyerly/Downloads/popcorn-compiler/test-install/aarch64/lib/libelf.a(32.xlatetof.o)"(.text.addr_32M__tof); /* size 0x468 */
	. = . + 0xd2; /* padding after .text.addr_32M__tof */
	. = ALIGN(0x10); /* align for .text.off_32L__tom */
	"/home/rlyerly/Downloads/popcorn-compiler/test-install/aarch64/lib/libelf.a(32.xlatetof.o)"(.text.off_32L__tom); /* size 0xd0 */
	. = . + 0x10; /* padding after .text.off_32L__tom */
	. = ALIGN(0x10); /* align for .text.off_32L__tof */
	"/home/rlyerly/Downloads/popcorn-compiler/test-install/aarch64/lib/libelf.a(32.xlatetof.o)"(.text.off_32L__tof); /* size 0x464 */
	. = . + 0xd6; /* padding after .text.off_32L__tof */
	. = ALIGN(0x10); /* align for .text.off_32M__tom */
	"/home/rlyerly/Downloads/popcorn-compiler/test-install/aarch64/lib/libelf.a(32.xlatetof.o)"(.text.off_32M__tom); /* size 0xe4 */
	. = ALIGN(0x10); /* align for .text.off_32M__tof */
	"/home/rlyerly/Downloads/popcorn-compiler/test-install/aarch64/lib/libelf.a(32.xlatetof.o)"(.text.off_32M__tof); /* size 0x468 */
	. = . + 0xd2; /* padding after .text.off_32M__tof */
	. = ALIGN(0x10); /* align for .text._elf32_xltsize */
	"/home/rlyerly/Downloads/popcorn-compiler/test-install/aarch64/lib/libelf.a(32.xlatetof.o)"(.text._elf32_xltsize); /* size 0xa4 */
	. = ALIGN(0x10); /* align for .text.elf32_xlatetom */
	"/home/rlyerly/Downloads/popcorn-compiler/test-install/aarch64/lib/libelf.a(32.xlatetof.o)"(.text.elf32_xlatetom); /* size 0x16c */
	. = ALIGN(0x10); /* align for .text.elf32_xlatetof */
	"/home/rlyerly/Downloads/popcorn-compiler/test-install/aarch64/lib/libelf.a(32.xlatetof.o)"(.text.elf32_xlatetof); /* size 0x16c */
	. = ALIGN(0x10); /* align for .text._elf_scn_type */
	"/home/rlyerly/Downloads/popcorn-compiler/test-install/aarch64/lib/libelf.a(cook.o)"(.text._elf_scn_type); /* size 0xc8 */
	. = ALIGN(0x10); /* align for .text._elf_xlatetom */
	"/home/rlyerly/Downloads/popcorn-compiler/test-install/aarch64/lib/libelf.a(cook.o)"(.text._elf_xlatetom); /* size 0x4c */
	. = ALIGN(0x10); /* align for .text._elf_cook */
	"/home/rlyerly/Downloads/popcorn-compiler/test-install/aarch64/lib/libelf.a(cook.o)"(.text._elf_cook); /* size 0x870 */
	. = . + 0x16e; /* padding after .text._elf_cook */
	. = ALIGN(0x10); /* align for .text._elf_read */
	"/home/rlyerly/Downloads/popcorn-compiler/test-install/aarch64/lib/libelf.a(input.o)"(.text._elf_read); /* size 0x16c */
	. = ALIGN(0x10); /* align for .text._elf_mmap */
	"/home/rlyerly/Downloads/popcorn-compiler/test-install/aarch64/lib/libelf.a(input.o)"(.text._elf_mmap); /* size 0x68 */
	. = ALIGN(0x10); /* align for .text.elf_getphdrnum */
	"/home/rlyerly/Downloads/popcorn-compiler/test-install/aarch64/lib/libelf.a(x.elfext.o)"(.text.elf_getphdrnum); /* size 0x80 */
	. = ALIGN(0x10); /* align for .text.elf_getshdrnum */
	"/home/rlyerly/Downloads/popcorn-compiler/test-install/aarch64/lib/libelf.a(x.elfext.o)"(.text.elf_getshdrnum); /* size 0x90 */
	. = ALIGN(0x10); /* align for .text.elf_getshdrstrndx */
	"/home/rlyerly/Downloads/popcorn-compiler/test-install/aarch64/lib/libelf.a(x.elfext.o)"(.text.elf_getshdrstrndx); /* size 0x140 */
	. = ALIGN(0x10); /* align for .text.elf_getphnum */
	"/home/rlyerly/Downloads/popcorn-compiler/test-install/aarch64/lib/libelf.a(x.elfext.o)"(.text.elf_getphnum); /* size 0x80 */
	. = ALIGN(0x10); /* align for .text.elf_getshnum */
	"/home/rlyerly/Downloads/popcorn-compiler/test-install/aarch64/lib/libelf.a(x.elfext.o)"(.text.elf_getshnum); /* size 0x90 */
	. = ALIGN(0x10); /* align for .text.elf_getshstrndx */
	"/home/rlyerly/Downloads/popcorn-compiler/test-install/aarch64/lib/libelf.a(x.elfext.o)"(.text.elf_getshstrndx); /* size 0x140 */
	. = ALIGN(0x10); /* align for .text.elfx_update_shstrndx */
	"/home/rlyerly/Downloads/popcorn-compiler/test-install/aarch64/lib/libelf.a(x.elfext.o)"(.text.elfx_update_shstrndx); /* size 0x11c */
	. = ALIGN(0x10); /* align for .text.half_64L__tom */
	"/home/rlyerly/Downloads/popcorn-compiler/test-install/aarch64/lib/libelf.a(64.xlatetof.o)"(.text.half_64L__tom); /* size 0x120 */
	. = ALIGN(0x10); /* align for .text.half_64L__tof */
	"/home/rlyerly/Downloads/popcorn-compiler/test-install/aarch64/lib/libelf.a(64.xlatetof.o)"(.text.half_64L__tof); /* size 0x29c */
	. = ALIGN(0x10); /* align for .text.half_64M__tom */
	"/home/rlyerly/Downloads/popcorn-compiler/test-install/aarch64/lib/libelf.a(64.xlatetof.o)"(.text.half_64M__tom); /* size 0x14c */
	. = . + 0x6; /* padding after .text.half_64M__tom */
	. = ALIGN(0x10); /* align for .text.half_64M__tof */
	"/home/rlyerly/Downloads/popcorn-compiler/test-install/aarch64/lib/libelf.a(64.xlatetof.o)"(.text.half_64M__tof); /* size 0x2a0 */
	. = ALIGN(0x10); /* align for .text.sword_64L__tom */
	"/home/rlyerly/Downloads/popcorn-compiler/test-install/aarch64/lib/libelf.a(64.xlatetof.o)"(.text.sword_64L__tom); /* size 0xd0 */
	. = . + 0x10; /* padding after .text.sword_64L__tom */
	. = ALIGN(0x10); /* align for .text.sword_64L__tof */
	"/home/rlyerly/Downloads/popcorn-compiler/test-install/aarch64/lib/libelf.a(64.xlatetof.o)"(.text.sword_64L__tof); /* size 0x464 */
	. = . + 0xd6; /* padding after .text.sword_64L__tof */
	. = ALIGN(0x10); /* align for .text.sword_64M__tom */
	"/home/rlyerly/Downloads/popcorn-compiler/test-install/aarch64/lib/libelf.a(64.xlatetof.o)"(.text.sword_64M__tom); /* size 0xe4 */
	. = ALIGN(0x10); /* align for .text.sword_64M__tof */
	"/home/rlyerly/Downloads/popcorn-compiler/test-install/aarch64/lib/libelf.a(64.xlatetof.o)"(.text.sword_64M__tof); /* size 0x468 */
	. = . + 0xd2; /* padding after .text.sword_64M__tof */
	. = ALIGN(0x10); /* align for .text.word_64L__tom */
	"/home/rlyerly/Downloads/popcorn-compiler/test-install/aarch64/lib/libelf.a(64.xlatetof.o)"(.text.word_64L__tom); /* size 0xd0 */
	. = . + 0x10; /* padding after .text.word_64L__tom */
	. = ALIGN(0x10); /* align for .text.word_64L__tof */
	"/home/rlyerly/Downloads/popcorn-compiler/test-install/aarch64/lib/libelf.a(64.xlatetof.o)"(.text.word_64L__tof); /* size 0x464 */
	. = . + 0xd6; /* padding after .text.word_64L__tof */
	. = ALIGN(0x10); /* align for .text.word_64M__tom */
	"/home/rlyerly/Downloads/popcorn-compiler/test-install/aarch64/lib/libelf.a(64.xlatetof.o)"(.text.word_64M__tom); /* size 0xe4 */
	. = ALIGN(0x10); /* align for .text.word_64M__tof */
	"/home/rlyerly/Downloads/popcorn-compiler/test-install/aarch64/lib/libelf.a(64.xlatetof.o)"(.text.word_64M__tof); /* size 0x468 */
	. = . + 0xd2; /* padding after .text.word_64M__tof */
	. = ALIGN(0x10); /* align for .text.elf64_xlate */
	"/home/rlyerly/Downloads/popcorn-compiler/test-install/aarch64/lib/libelf.a(64.xlatetof.o)"(.text.elf64_xlate); /* size 0x174 */
	. = ALIGN(0x10); /* align for .text.xword_64M__tof */
	"/home/rlyerly/Downloads/popcorn-compiler/test-install/aarch64/lib/libelf.a(64.xlatetof.o)"(.text.xword_64M__tof); /* size 0x60 */
	. = ALIGN(0x10); /* align for .text.sym_64M11_tof */
	"/home/rlyerly/Downloads/popcorn-compiler/test-install/aarch64/lib/libelf.a(64.xlatetof.o)"(.text.sym_64M11_tof); /* size 0xc0 */
	. = ALIGN(0x10); /* align for .text.shdr_64M11_tof */
	"/home/rlyerly/Downloads/popcorn-compiler/test-install/aarch64/lib/libelf.a(64.xlatetof.o)"(.text.shdr_64M11_tof); /* size 0x12c */
	. = ALIGN(0x10); /* align for .text.rel_64M11_tof */
	"/home/rlyerly/Downloads/popcorn-compiler/test-install/aarch64/lib/libelf.a(64.xlatetof.o)"(.text.rel_64M11_tof); /* size 0x70 */
	. = ALIGN(0x10); /* align for .text.phdr_64M11_tof */
	"/home/rlyerly/Downloads/popcorn-compiler/test-install/aarch64/lib/libelf.a(64.xlatetof.o)"(.text.phdr_64M11_tof); /* size 0x100 */
	. = ALIGN(0x10); /* align for .text.dyn_64M11_tof */
	"/home/rlyerly/Downloads/popcorn-compiler/test-install/aarch64/lib/libelf.a(64.xlatetof.o)"(.text.dyn_64M11_tof); /* size 0x70 */
	. = ALIGN(0x10); /* align for .text.xword_64M__tom */
	"/home/rlyerly/Downloads/popcorn-compiler/test-install/aarch64/lib/libelf.a(64.xlatetof.o)"(.text.xword_64M__tom); /* size 0x60 */
	. = ALIGN(0x10); /* align for .text.sym_64M11_tom */
	"/home/rlyerly/Downloads/popcorn-compiler/test-install/aarch64/lib/libelf.a(64.xlatetof.o)"(.text.sym_64M11_tom); /* size 0xa4 */
	. = ALIGN(0x10); /* align for .text.shdr_64M11_tom */
	"/home/rlyerly/Downloads/popcorn-compiler/test-install/aarch64/lib/libelf.a(64.xlatetof.o)"(.text.shdr_64M11_tom); /* size 0xcc */
	. = ALIGN(0x10); /* align for .text.rel_64M11_tom */
	"/home/rlyerly/Downloads/popcorn-compiler/test-install/aarch64/lib/libelf.a(64.xlatetof.o)"(.text.rel_64M11_tom); /* size 0x70 */
	. = ALIGN(0x10); /* align for .text.phdr_64M11_tom */
	"/home/rlyerly/Downloads/popcorn-compiler/test-install/aarch64/lib/libelf.a(64.xlatetof.o)"(.text.phdr_64M11_tom); /* size 0xd0 */
	. = ALIGN(0x10); /* align for .text.dyn_64M11_tom */
	"/home/rlyerly/Downloads/popcorn-compiler/test-install/aarch64/lib/libelf.a(64.xlatetof.o)"(.text.dyn_64M11_tom); /* size 0x70 */
	. = ALIGN(0x10); /* align for .text.sxword_64M__tof */
	"/home/rlyerly/Downloads/popcorn-compiler/test-install/aarch64/lib/libelf.a(64.xlatetof.o)"(.text.sxword_64M__tof); /* size 0x60 */
	. = ALIGN(0x10); /* align for .text.rela_64M11_tof */
	"/home/rlyerly/Downloads/popcorn-compiler/test-install/aarch64/lib/libelf.a(64.xlatetof.o)"(.text.rela_64M11_tof); /* size 0x8c */
	. = ALIGN(0x10); /* align for .text.sxword_64M__tom */
	"/home/rlyerly/Downloads/popcorn-compiler/test-install/aarch64/lib/libelf.a(64.xlatetof.o)"(.text.sxword_64M__tom); /* size 0x60 */
	. = ALIGN(0x10); /* align for .text.rela_64M11_tom */
	"/home/rlyerly/Downloads/popcorn-compiler/test-install/aarch64/lib/libelf.a(64.xlatetof.o)"(.text.rela_64M11_tom); /* size 0x8c */
	. = ALIGN(0x10); /* align for .text.xword_64L__tof */
	"/home/rlyerly/Downloads/popcorn-compiler/test-install/aarch64/lib/libelf.a(64.xlatetof.o)"(.text.xword_64L__tof); /* size 0x60 */
	. = ALIGN(0x10); /* align for .text.sym_64L11_tof */
	"/home/rlyerly/Downloads/popcorn-compiler/test-install/aarch64/lib/libelf.a(64.xlatetof.o)"(.text.sym_64L11_tof); /* size 0xc0 */
	. = ALIGN(0x10); /* align for .text.shdr_64L11_tof */
	"/home/rlyerly/Downloads/popcorn-compiler/test-install/aarch64/lib/libelf.a(64.xlatetof.o)"(.text.shdr_64L11_tof); /* size 0x12c */
	. = ALIGN(0x10); /* align for .text.rel_64L11_tof */
	"/home/rlyerly/Downloads/popcorn-compiler/test-install/aarch64/lib/libelf.a(64.xlatetof.o)"(.text.rel_64L11_tof); /* size 0x70 */
	. = ALIGN(0x10); /* align for .text.phdr_64L11_tof */
	"/home/rlyerly/Downloads/popcorn-compiler/test-install/aarch64/lib/libelf.a(64.xlatetof.o)"(.text.phdr_64L11_tof); /* size 0x100 */
	. = ALIGN(0x10); /* align for .text.dyn_64L11_tof */
	"/home/rlyerly/Downloads/popcorn-compiler/test-install/aarch64/lib/libelf.a(64.xlatetof.o)"(.text.dyn_64L11_tof); /* size 0x70 */
	. = ALIGN(0x10); /* align for .text.xword_64L__tom */
	"/home/rlyerly/Downloads/popcorn-compiler/test-install/aarch64/lib/libelf.a(64.xlatetof.o)"(.text.xword_64L__tom); /* size 0x60 */
	. = ALIGN(0x10); /* align for .text.sym_64L11_tom */
	"/home/rlyerly/Downloads/popcorn-compiler/test-install/aarch64/lib/libelf.a(64.xlatetof.o)"(.text.sym_64L11_tom); /* size 0x9c */
	. = ALIGN(0x10); /* align for .text.shdr_64L11_tom */
	"/home/rlyerly/Downloads/popcorn-compiler/test-install/aarch64/lib/libelf.a(64.xlatetof.o)"(.text.shdr_64L11_tom); /* size 0xbc */
	. = ALIGN(0x10); /* align for .text.rel_64L11_tom */
	"/home/rlyerly/Downloads/popcorn-compiler/test-install/aarch64/lib/libelf.a(64.xlatetof.o)"(.text.rel_64L11_tom); /* size 0x70 */
	. = ALIGN(0x10); /* align for .text.phdr_64L11_tom */
	"/home/rlyerly/Downloads/popcorn-compiler/test-install/aarch64/lib/libelf.a(64.xlatetof.o)"(.text.phdr_64L11_tom); /* size 0xc8 */
	. = ALIGN(0x10); /* align for .text.dyn_64L11_tom */
	"/home/rlyerly/Downloads/popcorn-compiler/test-install/aarch64/lib/libelf.a(64.xlatetof.o)"(.text.dyn_64L11_tom); /* size 0x70 */
	. = ALIGN(0x10); /* align for .text.sxword_64L__tof */
	"/home/rlyerly/Downloads/popcorn-compiler/test-install/aarch64/lib/libelf.a(64.xlatetof.o)"(.text.sxword_64L__tof); /* size 0x60 */
	. = ALIGN(0x10); /* align for .text.rela_64L11_tof */
	"/home/rlyerly/Downloads/popcorn-compiler/test-install/aarch64/lib/libelf.a(64.xlatetof.o)"(.text.rela_64L11_tof); /* size 0x8c */
	. = ALIGN(0x10); /* align for .text.sxword_64L__tom */
	"/home/rlyerly/Downloads/popcorn-compiler/test-install/aarch64/lib/libelf.a(64.xlatetof.o)"(.text.sxword_64L__tom); /* size 0x60 */
	. = ALIGN(0x10); /* align for .text.rela_64L11_tom */
	"/home/rlyerly/Downloads/popcorn-compiler/test-install/aarch64/lib/libelf.a(64.xlatetof.o)"(.text.rela_64L11_tom); /* size 0x8c */
	. = ALIGN(0x10); /* align for .text.ehdr_64M11_tom */
	"/home/rlyerly/Downloads/popcorn-compiler/test-install/aarch64/lib/libelf.a(64.xlatetof.o)"(.text.ehdr_64M11_tom); /* size 0x108 */
	. = ALIGN(0x10); /* align for .text.byte_copy */
	"/home/rlyerly/Downloads/popcorn-compiler/test-install/aarch64/lib/libelf.a(64.xlatetof.o)"(.text.byte_copy); /* size 0x34 */
	. = ALIGN(0x10); /* align for .text.ehdr_64M11_tof */
	"/home/rlyerly/Downloads/popcorn-compiler/test-install/aarch64/lib/libelf.a(64.xlatetof.o)"(.text.ehdr_64M11_tof); /* size 0x158 */
	. = ALIGN(0x10); /* align for .text.ehdr_64L11_tom */
	"/home/rlyerly/Downloads/popcorn-compiler/test-install/aarch64/lib/libelf.a(64.xlatetof.o)"(.text.ehdr_64L11_tom); /* size 0xe0 */
	. = . + 0x3; /* padding after .text.ehdr_64L11_tom */
	. = ALIGN(0x10); /* align for .text.ehdr_64L11_tof */
	"/home/rlyerly/Downloads/popcorn-compiler/test-install/aarch64/lib/libelf.a(64.xlatetof.o)"(.text.ehdr_64L11_tof); /* size 0x158 */
	. = ALIGN(0x10); /* align for .text.addr_64L__tom */
	"/home/rlyerly/Downloads/popcorn-compiler/test-install/aarch64/lib/libelf.a(64.xlatetof.o)"(.text.addr_64L__tom); /* size 0x60 */
	. = ALIGN(0x10); /* align for .text.addr_64L__tof */
	"/home/rlyerly/Downloads/popcorn-compiler/test-install/aarch64/lib/libelf.a(64.xlatetof.o)"(.text.addr_64L__tof); /* size 0x60 */
	. = ALIGN(0x10); /* align for .text.addr_64M__tom */
	"/home/rlyerly/Downloads/popcorn-compiler/test-install/aarch64/lib/libelf.a(64.xlatetof.o)"(.text.addr_64M__tom); /* size 0x60 */
	. = ALIGN(0x10); /* align for .text.addr_64M__tof */
	"/home/rlyerly/Downloads/popcorn-compiler/test-install/aarch64/lib/libelf.a(64.xlatetof.o)"(.text.addr_64M__tof); /* size 0x60 */
	. = ALIGN(0x10); /* align for .text.off_64L__tom */
	"/home/rlyerly/Downloads/popcorn-compiler/test-install/aarch64/lib/libelf.a(64.xlatetof.o)"(.text.off_64L__tom); /* size 0x60 */
	. = ALIGN(0x10); /* align for .text.off_64L__tof */
	"/home/rlyerly/Downloads/popcorn-compiler/test-install/aarch64/lib/libelf.a(64.xlatetof.o)"(.text.off_64L__tof); /* size 0x60 */
	. = ALIGN(0x10); /* align for .text.off_64M__tom */
	"/home/rlyerly/Downloads/popcorn-compiler/test-install/aarch64/lib/libelf.a(64.xlatetof.o)"(.text.off_64M__tom); /* size 0x60 */
	. = ALIGN(0x10); /* align for .text.off_64M__tof */
	"/home/rlyerly/Downloads/popcorn-compiler/test-install/aarch64/lib/libelf.a(64.xlatetof.o)"(.text.off_64M__tof); /* size 0x60 */
	. = ALIGN(0x10); /* align for .text._elf64_xltsize */
	"/home/rlyerly/Downloads/popcorn-compiler/test-install/aarch64/lib/libelf.a(64.xlatetof.o)"(.text._elf64_xltsize); /* size 0xa4 */
	. = ALIGN(0x10); /* align for .text.elf64_xlatetom */
	"/home/rlyerly/Downloads/popcorn-compiler/test-install/aarch64/lib/libelf.a(64.xlatetof.o)"(.text.elf64_xlatetom); /* size 0x16c */
	. = ALIGN(0x10); /* align for .text.elf64_xlatetof */
	"/home/rlyerly/Downloads/popcorn-compiler/test-install/aarch64/lib/libelf.a(64.xlatetof.o)"(.text.elf64_xlatetof); /* size 0x16c */
	. = ALIGN(0x10); /* align for .text.gelf_xlatetom */
	"/home/rlyerly/Downloads/popcorn-compiler/test-install/aarch64/lib/libelf.a(64.xlatetof.o)"(.text.gelf_xlatetom); /* size 0x78 */
	. = ALIGN(0x10); /* align for .text.gelf_xlatetof */
	"/home/rlyerly/Downloads/popcorn-compiler/test-install/aarch64/lib/libelf.a(64.xlatetof.o)"(.text.gelf_xlatetof); /* size 0x78 */
	. = ALIGN(0x10); /* align for .text.gelf_getshdr */
	"/home/rlyerly/Downloads/popcorn-compiler/test-install/aarch64/lib/libelf.a(gelfshdr.o)"(.text.gelf_getshdr); /* size 0x138 */
	. = . + 0x53; /* padding after .text.gelf_getshdr */
	. = ALIGN(0x10); /* align for .text.gelf_update_shdr */
	"/home/rlyerly/Downloads/popcorn-compiler/test-install/aarch64/lib/libelf.a(gelfshdr.o)"(.text.gelf_update_shdr); /* size 0x100 */
	. = . + 0x13; /* padding after .text.gelf_update_shdr */
	. = ALIGN(0x10); /* align for .text._elf_load_u64L */
	"/home/rlyerly/Downloads/popcorn-compiler/test-install/aarch64/lib/libelf.a(swap64.o)"(.text._elf_load_u64L); /* size 0x8 */
	. = ALIGN(0x10); /* align for .text._elf_load_u64M */
	"/home/rlyerly/Downloads/popcorn-compiler/test-install/aarch64/lib/libelf.a(swap64.o)"(.text._elf_load_u64M); /* size 0xc */
	. = ALIGN(0x10); /* align for .text._elf_load_i64L */
	"/home/rlyerly/Downloads/popcorn-compiler/test-install/aarch64/lib/libelf.a(swap64.o)"(.text._elf_load_i64L); /* size 0x8 */
	. = ALIGN(0x10); /* align for .text._elf_load_i64M */
	"/home/rlyerly/Downloads/popcorn-compiler/test-install/aarch64/lib/libelf.a(swap64.o)"(.text._elf_load_i64M); /* size 0xc */
	. = ALIGN(0x10); /* align for .text._elf_store_u64L */
	"/home/rlyerly/Downloads/popcorn-compiler/test-install/aarch64/lib/libelf.a(swap64.o)"(.text._elf_store_u64L); /* size 0x40 */
	. = ALIGN(0x10); /* align for .text._elf_store_u64M */
	"/home/rlyerly/Downloads/popcorn-compiler/test-install/aarch64/lib/libelf.a(swap64.o)"(.text._elf_store_u64M); /* size 0x40 */
	. = ALIGN(0x10); /* align for .text._elf_store_i64L */
	"/home/rlyerly/Downloads/popcorn-compiler/test-install/aarch64/lib/libelf.a(swap64.o)"(.text._elf_store_i64L); /* size 0x40 */
	. = ALIGN(0x10); /* align for .text._elf_store_i64M */
	"/home/rlyerly/Downloads/popcorn-compiler/test-install/aarch64/lib/libelf.a(swap64.o)"(.text._elf_store_i64M); /* size 0x40 */
	. = ALIGN(0x10); /* align for .text._elf_verdef_32L11_tof */
	"/home/rlyerly/Downloads/popcorn-compiler/test-install/aarch64/lib/libelf.a(verdef_32_tof.o)"(.text._elf_verdef_32L11_tof); /* size 0x1e8 */
	. = . + 0x3f; /* padding after .text._elf_verdef_32L11_tof */
	. = ALIGN(0x10); /* align for .text._elf_verdef_32M11_tof */
	"/home/rlyerly/Downloads/popcorn-compiler/test-install/aarch64/lib/libelf.a(verdef_32_tof.o)"(.text._elf_verdef_32M11_tof); /* size 0x1e8 */
	. = . + 0x3f; /* padding after .text._elf_verdef_32M11_tof */
	. = ALIGN(0x10); /* align for .text._elf_verneed_32L11_tof */
	"/home/rlyerly/Downloads/popcorn-compiler/test-install/aarch64/lib/libelf.a(verdef_32_tof.o)"(.text._elf_verneed_32L11_tof); /* size 0x21c */
	. = . + 0x4b; /* padding after .text._elf_verneed_32L11_tof */
	. = ALIGN(0x10); /* align for .text._elf_verneed_32M11_tof */
	"/home/rlyerly/Downloads/popcorn-compiler/test-install/aarch64/lib/libelf.a(verdef_32_tof.o)"(.text._elf_verneed_32M11_tof); /* size 0x224 */
	. = . + 0x43; /* padding after .text._elf_verneed_32M11_tof */
	. = ALIGN(0x10); /* align for .text._elf_verdef_32L11_tom */
	"/home/rlyerly/Downloads/popcorn-compiler/test-install/aarch64/lib/libelf.a(verdef_32_tom.o)"(.text._elf_verdef_32L11_tom); /* size 0x128 */
	. = . + 0xf; /* padding after .text._elf_verdef_32L11_tom */
	. = ALIGN(0x10); /* align for .text._elf_verdef_32M11_tom */
	"/home/rlyerly/Downloads/popcorn-compiler/test-install/aarch64/lib/libelf.a(verdef_32_tom.o)"(.text._elf_verdef_32M11_tom); /* size 0x158 */
	. = . + 0xf; /* padding after .text._elf_verdef_32M11_tom */
	. = ALIGN(0x10); /* align for .text._elf_verneed_32L11_tom */
	"/home/rlyerly/Downloads/popcorn-compiler/test-install/aarch64/lib/libelf.a(verdef_32_tom.o)"(.text._elf_verneed_32L11_tom); /* size 0x130 */
	. = . + 0x17; /* padding after .text._elf_verneed_32L11_tom */
	. = ALIGN(0x10); /* align for .text._elf_verneed_32M11_tom */
	"/home/rlyerly/Downloads/popcorn-compiler/test-install/aarch64/lib/libelf.a(verdef_32_tom.o)"(.text._elf_verneed_32M11_tom); /* size 0x158 */
	. = . + 0x1f; /* padding after .text._elf_verneed_32M11_tom */
	. = ALIGN(0x10); /* align for .text._elf_verdef_64L11_tof */
	"/home/rlyerly/Downloads/popcorn-compiler/test-install/aarch64/lib/libelf.a(verdef_64_tof.o)"(.text._elf_verdef_64L11_tof); /* size 0x1e8 */
	. = . + 0x3f; /* padding after .text._elf_verdef_64L11_tof */
	. = ALIGN(0x10); /* align for .text._elf_verdef_64M11_tof */
	"/home/rlyerly/Downloads/popcorn-compiler/test-install/aarch64/lib/libelf.a(verdef_64_tof.o)"(.text._elf_verdef_64M11_tof); /* size 0x1e8 */
	. = . + 0x3f; /* padding after .text._elf_verdef_64M11_tof */
	. = ALIGN(0x10); /* align for .text._elf_verneed_64L11_tof */
	"/home/rlyerly/Downloads/popcorn-compiler/test-install/aarch64/lib/libelf.a(verdef_64_tof.o)"(.text._elf_verneed_64L11_tof); /* size 0x21c */
	. = . + 0x4b; /* padding after .text._elf_verneed_64L11_tof */
	. = ALIGN(0x10); /* align for .text._elf_verneed_64M11_tof */
	"/home/rlyerly/Downloads/popcorn-compiler/test-install/aarch64/lib/libelf.a(verdef_64_tof.o)"(.text._elf_verneed_64M11_tof); /* size 0x224 */
	. = . + 0x43; /* padding after .text._elf_verneed_64M11_tof */
	. = ALIGN(0x10); /* align for .text._elf_verdef_64L11_tom */
	"/home/rlyerly/Downloads/popcorn-compiler/test-install/aarch64/lib/libelf.a(verdef_64_tom.o)"(.text._elf_verdef_64L11_tom); /* size 0x128 */
	. = . + 0xf; /* padding after .text._elf_verdef_64L11_tom */
	. = ALIGN(0x10); /* align for .text._elf_verdef_64M11_tom */
	"/home/rlyerly/Downloads/popcorn-compiler/test-install/aarch64/lib/libelf.a(verdef_64_tom.o)"(.text._elf_verdef_64M11_tom); /* size 0x158 */
	. = . + 0xf; /* padding after .text._elf_verdef_64M11_tom */
	. = ALIGN(0x10); /* align for .text._elf_verneed_64L11_tom */
	"/home/rlyerly/Downloads/popcorn-compiler/test-install/aarch64/lib/libelf.a(verdef_64_tom.o)"(.text._elf_verneed_64L11_tom); /* size 0x130 */
	. = . + 0x17; /* padding after .text._elf_verneed_64L11_tom */
	. = ALIGN(0x10); /* align for .text._elf_verneed_64M11_tom */
	"/home/rlyerly/Downloads/popcorn-compiler/test-install/aarch64/lib/libelf.a(verdef_64_tom.o)"(.text._elf_verneed_64M11_tom); /* size 0x158 */
	. = . + 0x1f; /* padding after .text._elf_verneed_64M11_tom */
	. = ALIGN(0x10); /* align for .text.elf_getscn */
	"/home/rlyerly/Downloads/popcorn-compiler/test-install/aarch64/lib/libelf.a(getscn.o)"(.text.elf_getscn); /* size 0xa4 */
	. = ALIGN(0x10); /* align for .text._elf_update_shnum */
	"/home/rlyerly/Downloads/popcorn-compiler/test-install/aarch64/lib/libelf.a(newscn.o)"(.text._elf_update_shnum); /* size 0x88 */
	. = ALIGN(0x10); /* align for .text._elf_first_scn */
	"/home/rlyerly/Downloads/popcorn-compiler/test-install/aarch64/lib/libelf.a(newscn.o)"(.text._elf_first_scn); /* size 0xe4 */
	. = . + 0x24; /* padding after .text._elf_first_scn */
	. = ALIGN(0x10); /* align for .text.elf_newscn */
	"/home/rlyerly/Downloads/popcorn-compiler/test-install/aarch64/lib/libelf.a(newscn.o)"(.text.elf_newscn); /* size 0x25c */
	. = . + 0x31; /* padding after .text.elf_newscn */
	. = ALIGN(0x10); /* align for .text.elf32_fsize */
	"/home/rlyerly/Downloads/popcorn-compiler/test-install/aarch64/lib/libelf.a(32.fsize.o)"(.text.elf32_fsize); /* size 0x60 */
	. = ALIGN(0x10); /* align for .text.elf64_fsize */
	"/home/rlyerly/Downloads/popcorn-compiler/test-install/aarch64/lib/libelf.a(32.fsize.o)"(.text.elf64_fsize); /* size 0x60 */
	. = ALIGN(0x10); /* align for .text.gelf_fsize */
	"/home/rlyerly/Downloads/popcorn-compiler/test-install/aarch64/lib/libelf.a(32.fsize.o)"(.text.gelf_fsize); /* size 0xb8 */
	. = ALIGN(0x10); /* align for .text.gelf_msize */
	"/home/rlyerly/Downloads/popcorn-compiler/test-install/aarch64/lib/libelf.a(32.fsize.o)"(.text.gelf_msize); /* size 0xbc */
	. = ALIGN(0x4); /* align for .text.__init_ssp */
	"/home/rlyerly/Downloads/popcorn-compiler/test-install/aarch64/lib/libc.a(__stack_chk_fail.o)"(.text.__init_ssp); /* size 0x60 */
	. = ALIGN(0x4); /* align for .text.__stack_chk_fail */
	"/home/rlyerly/Downloads/popcorn-compiler/test-install/aarch64/lib/libc.a(__stack_chk_fail.o)"(.text.__stack_chk_fail); /* size 0xc */
	. = ALIGN(0x4); /* align for .text.getenv */
	"/home/rlyerly/Downloads/popcorn-compiler/test-install/aarch64/lib/libc.a(getenv.o)"(.text.getenv); /* size 0x8c */
	. = ALIGN(0x4); /* align for .text.__assert_fail */
	"/home/rlyerly/Downloads/popcorn-compiler/test-install/aarch64/lib/libc.a(assert.o)"(.text.__assert_fail); /* size 0x4c */
	. = ALIGN(0x4); /* align for .text.open */
	"/home/rlyerly/Downloads/popcorn-compiler/test-install/aarch64/lib/libc.a(open.o)"(.text.open); /* size 0x10c */
	. = . + 0x1a; /* padding after .text.open */
	. = ALIGN(0x10); /* align for .text.calloc */
	"/home/rlyerly/Downloads/popcorn-compiler/test-install/aarch64/lib/libc.a(calloc.o)"(.text.calloc); /* size 0x48 */
	. = ALIGN(0x10); /* align for .text.__simple_malloc */
	"/home/rlyerly/Downloads/popcorn-compiler/test-install/aarch64/lib/libc.a(lite_malloc.o)"(.text.__simple_malloc); /* size 0x104 */
	. = ALIGN(0x10); /* align for .text.malloc */
	"/home/rlyerly/Downloads/popcorn-compiler/test-install/aarch64/lib/libc.a(malloc.o)"(.text.malloc); /* size 0x828 */
	. = ALIGN(0x10); /* align for .text.lock_bin */
	"/home/rlyerly/Downloads/popcorn-compiler/test-install/aarch64/lib/libc.a(malloc.o)"(.text.lock_bin); /* size 0xac */
	. = ALIGN(0x10); /* align for .text.unbin */
	"/home/rlyerly/Downloads/popcorn-compiler/test-install/aarch64/lib/libc.a(malloc.o)"(.text.unbin); /* size 0xa4 */
	. = ALIGN(0x10); /* align for .text.unlock_bin */
	"/home/rlyerly/Downloads/popcorn-compiler/test-install/aarch64/lib/libc.a(malloc.o)"(.text.unlock_bin); /* size 0x6c */
	. = ALIGN(0x10); /* align for .text.__malloc0 */
	"/home/rlyerly/Downloads/popcorn-compiler/test-install/aarch64/lib/libc.a(malloc.o)"(.text.__malloc0); /* size 0xc4 */
	. = ALIGN(0x10); /* align for .text.realloc */
	"/home/rlyerly/Downloads/popcorn-compiler/test-install/aarch64/lib/libc.a(malloc.o)"(.text.realloc); /* size 0x210 */
	. = ALIGN(0x10); /* align for .text.free */
	"/home/rlyerly/Downloads/popcorn-compiler/test-install/aarch64/lib/libc.a(malloc.o)"(.text.free); /* size 0x684 */
	. = ALIGN(0x10); /* align for .text.alloc_fwd */
	"/home/rlyerly/Downloads/popcorn-compiler/test-install/aarch64/lib/libc.a(malloc.o)"(.text.alloc_fwd); /* size 0xc8 */
	. = ALIGN(0x4); /* align for .text.getrlimit */
	"/home/rlyerly/Downloads/popcorn-compiler/test-install/aarch64/lib/libc.a(getrlimit.o)"(.text.getrlimit); /* size 0xf8 */
	. = ALIGN(0x4); /* align for .text.syscall */
	"/home/rlyerly/Downloads/popcorn-compiler/test-install/aarch64/lib/libc.a(syscall.o)"(.text.syscall); /* size 0x188 */
	. = . + 0x67; /* padding after .text.syscall */
	. = ALIGN(0x4); /* align for .text.__madvise */
	"/home/rlyerly/Downloads/popcorn-compiler/test-install/aarch64/lib/libc.a(madvise.o)"(.text.__madvise); /* size 0x20 */
	. = ALIGN(0x4); /* align for .text.dummy */
	"/home/rlyerly/Downloads/popcorn-compiler/test-install/aarch64/lib/libc.a(mmap.o)"(.text.dummy); /* size 0x4 */
	. = ALIGN(0x4); /* align for .text.__mmap */
	"/home/rlyerly/Downloads/popcorn-compiler/test-install/aarch64/lib/libc.a(mmap.o)"(.text.__mmap); /* size 0xc0 */
	. = ALIGN(0x4); /* align for .text.dummy */
	"/home/rlyerly/Downloads/popcorn-compiler/test-install/aarch64/lib/libc.a(mremap.o)"(.text.dummy); /* size 0x4 */
	. = ALIGN(0x4); /* align for .text.__mremap */
	"/home/rlyerly/Downloads/popcorn-compiler/test-install/aarch64/lib/libc.a(mremap.o)"(.text.__mremap); /* size 0x110 */
	. = . + 0xe; /* padding after .text.__mremap */
	. = ALIGN(0x4); /* align for .text.dummy */
	"/home/rlyerly/Downloads/popcorn-compiler/test-install/aarch64/lib/libc.a(munmap.o)"(.text.dummy); /* size 0x4 */
	. = ALIGN(0x4); /* align for .text.__munmap */
	"/home/rlyerly/Downloads/popcorn-compiler/test-install/aarch64/lib/libc.a(munmap.o)"(.text.__munmap); /* size 0x38 */
	. = ALIGN(0x4); /* align for .text.__get_handler_set */
	"/home/rlyerly/Downloads/popcorn-compiler/test-install/aarch64/lib/libc.a(sigaction.o)"(.text.__get_handler_set); /* size 0x10 */
	. = ALIGN(0x4); /* align for .text.__libc_sigaction */
	"/home/rlyerly/Downloads/popcorn-compiler/test-install/aarch64/lib/libc.a(sigaction.o)"(.text.__libc_sigaction); /* size 0x194 */
	. = ALIGN(0x4); /* align for .text.__sigaction */
	"/home/rlyerly/Downloads/popcorn-compiler/test-install/aarch64/lib/libc.a(sigaction.o)"(.text.__sigaction); /* size 0x40 */
	. = ALIGN(0x4); /* align for .text.sigemptyset */
	"/home/rlyerly/Downloads/popcorn-compiler/test-install/aarch64/lib/libc.a(sigemptyset.o)"(.text.sigemptyset); /* size 0xc */
	. = ALIGN(0x4); /* align for .text.__libc_current_sigrtmin */
	"/home/rlyerly/Downloads/popcorn-compiler/test-install/aarch64/lib/libc.a(sigrtmin.o)"(.text.__libc_current_sigrtmin); /* size 0x8 */
	. = ALIGN(0x1); /* align for .text.__restore_rt */
	"/home/rlyerly/Downloads/popcorn-compiler/test-install/aarch64/lib/libc.a(restore.o)"(.text.__restore_rt); /* size 0x8 */
	. = . + 0x1; /* padding after .text.__restore_rt */
	. = ALIGN(0x4); /* align for .text.dummy */
	"/home/rlyerly/Downloads/popcorn-compiler/test-install/aarch64/lib/libc.a(fclose.o)"(.text.dummy); /* size 0x4 */
	. = ALIGN(0x4); /* align for .text.fclose */
	"/home/rlyerly/Downloads/popcorn-compiler/test-install/aarch64/lib/libc.a(fclose.o)"(.text.fclose); /* size 0xd8 */
	. = ALIGN(0x4); /* align for .text.fflush */
	"/home/rlyerly/Downloads/popcorn-compiler/test-install/aarch64/lib/libc.a(fflush.o)"(.text.fflush); /* size 0x154 */
	. = ALIGN(0x4); /* align for .text.fopen */
	"/home/rlyerly/Downloads/popcorn-compiler/test-install/aarch64/lib/libc.a(fopen.o)"(.text.fopen); /* size 0xc0 */
	. = ALIGN(0x4); /* align for .text.fprintf */
	"/home/rlyerly/Downloads/popcorn-compiler/test-install/aarch64/lib/libc.a(fprintf.o)"(.text.fprintf); /* size 0x74 */
	. = . + 0x23; /* padding after .text.fprintf */
	. = ALIGN(0x4); /* align for .text.getline */
	"/home/rlyerly/Downloads/popcorn-compiler/test-install/aarch64/lib/libc.a(getline.o)"(.text.getline); /* size 0x10 */
	. = ALIGN(0x4); /* align for .text.perror */
	"/home/rlyerly/Downloads/popcorn-compiler/test-install/aarch64/lib/libc.a(perror.o)"(.text.perror); /* size 0xcc */
	. = ALIGN(0x4); /* align for .text.snprintf */
	"/home/rlyerly/Downloads/popcorn-compiler/test-install/aarch64/lib/libc.a(snprintf.o)"(.text.snprintf); /* size 0x74 */
	. = . + 0x1e; /* padding after .text.snprintf */
	. = ALIGN(0x4); /* align for .text.sscanf */
	"/home/rlyerly/Downloads/popcorn-compiler/test-install/aarch64/lib/libc.a(sscanf.o)"(.text.sscanf); /* size 0x74 */
	. = . + 0x23; /* padding after .text.sscanf */
	. = ALIGN(0x4); /* align for .text.vsnprintf */
	"/home/rlyerly/Downloads/popcorn-compiler/test-install/aarch64/lib/libc.a(vsnprintf.o)"(.text.vsnprintf); /* size 0xd4 */
	. = ALIGN(0x4); /* align for .text.sn_write */
	"/home/rlyerly/Downloads/popcorn-compiler/test-install/aarch64/lib/libc.a(vsnprintf.o)"(.text.sn_write); /* size 0xd8 */
	. = ALIGN(0x4); /* align for .text.vsscanf */
	"/home/rlyerly/Downloads/popcorn-compiler/test-install/aarch64/lib/libc.a(vsscanf.o)"(.text.vsscanf); /* size 0x80 */
	. = ALIGN(0x4); /* align for .text.do_read */
	"/home/rlyerly/Downloads/popcorn-compiler/test-install/aarch64/lib/libc.a(vsscanf.o)"(.text.do_read); /* size 0x4 */
	. = . + 0x1; /* padding after .text.do_read */
	. = ALIGN(0x4); /* align for .text.strtoull */
	"/home/rlyerly/Downloads/popcorn-compiler/test-install/aarch64/lib/libc.a(strtol.o)"(.text.strtoull); /* size 0x8 */
	. = . + 0x4; /* padding after .text.strtoull */
	. = ALIGN(0x4); /* align for .text.strtox */
	"/home/rlyerly/Downloads/popcorn-compiler/test-install/aarch64/lib/libc.a(strtol.o)"(.text.strtox); /* size 0xb4 */
	. = ALIGN(0x4); /* align for .text.strtoll */
	"/home/rlyerly/Downloads/popcorn-compiler/test-install/aarch64/lib/libc.a(strtol.o)"(.text.strtoll); /* size 0x8 */
	. = . + 0x7; /* padding after .text.strtoll */
	. = ALIGN(0x4); /* align for .text.strtoul */
	"/home/rlyerly/Downloads/popcorn-compiler/test-install/aarch64/lib/libc.a(strtol.o)"(.text.strtoul); /* size 0x8 */
	. = . + 0x4; /* padding after .text.strtoul */
	. = ALIGN(0x4); /* align for .text.strtol */
	"/home/rlyerly/Downloads/popcorn-compiler/test-install/aarch64/lib/libc.a(strtol.o)"(.text.strtol); /* size 0x8 */
	. = . + 0x7; /* padding after .text.strtol */
	. = ALIGN(0x4); /* align for .text.strtoimax */
	"/home/rlyerly/Downloads/popcorn-compiler/test-install/aarch64/lib/libc.a(strtol.o)"(.text.strtoimax); /* size 0x8 */
	. = . + 0x7; /* padding after .text.strtoimax */
	. = ALIGN(0x4); /* align for .text.strtoumax */
	"/home/rlyerly/Downloads/popcorn-compiler/test-install/aarch64/lib/libc.a(strtol.o)"(.text.strtoumax); /* size 0x8 */
	. = . + 0x4; /* padding after .text.strtoumax */
	. = ALIGN(0x10); /* align for .text.memcmp */
	"/home/rlyerly/Downloads/popcorn-compiler/test-install/aarch64/lib/libc.a(memcmp.o)"(.text.memcmp); /* size 0x30 */
	. = . + 0x2; /* padding after .text.memcmp */
	. = ALIGN(0x10); /* align for .text.strchr */
	"/home/rlyerly/Downloads/popcorn-compiler/test-install/aarch64/lib/libc.a(strchr.o)"(.text.strchr); /* size 0x2c */
	. = ALIGN(0x10); /* align for .text.__strchrnul */
	"/home/rlyerly/Downloads/popcorn-compiler/test-install/aarch64/lib/libc.a(strchrnul.o)"(.text.__strchrnul); /* size 0xac */
	. = . + 0x3c; /* padding after .text.__strchrnul */
	. = ALIGN(0x10); /* align for .text.strcmp */
	"/home/rlyerly/Downloads/popcorn-compiler/test-install/aarch64/lib/libc.a(strcmp.o)"(.text.strcmp); /* size 0x28 */
	. = . + 0x1e; /* padding after .text.strcmp */
	. = ALIGN(0x10); /* align for .text.strlen */
	"/home/rlyerly/Downloads/popcorn-compiler/test-install/aarch64/lib/libc.a(strlen.o)"(.text.strlen); /* size 0x60 */
	. = . + 0x1; /* padding after .text.strlen */
	. = ALIGN(0x10); /* align for .text.strncmp */
	"/home/rlyerly/Downloads/popcorn-compiler/test-install/aarch64/lib/libc.a(strncmp.o)"(.text.strncmp); /* size 0x50 */
	. = ALIGN(0x4); /* align for .text.memmove */
	"/home/rlyerly/Downloads/popcorn-compiler/test-install/aarch64/lib/libc.a(memmove.o)"(.text.memmove); /* size 0x214 */
	. = ALIGN(0x4); /* align for .text.sccp */
	"/home/rlyerly/Downloads/popcorn-compiler/test-install/aarch64/lib/libc.a(__syscall_cp.o)"(.text.sccp); /* size 0x4 */
	. = . + 0xf; /* padding after .text.sccp */
	. = ALIGN(0x4); /* align for .text.__syscall_cp */
	"/home/rlyerly/Downloads/popcorn-compiler/test-install/aarch64/lib/libc.a(__syscall_cp.o)"(.text.__syscall_cp); /* size 0x4 */
	. = . + 0x1; /* padding after .text.__syscall_cp */
	. = ALIGN(0x4); /* align for .text.pthread_attr_getdetachstate */
	"/home/rlyerly/Downloads/popcorn-compiler/test-install/aarch64/lib/libc.a(pthread_attr_get.o)"(.text.pthread_attr_getdetachstate); /* size 0x10 */
	. = ALIGN(0x4); /* align for .text.pthread_attr_getguardsize */
	"/home/rlyerly/Downloads/popcorn-compiler/test-install/aarch64/lib/libc.a(pthread_attr_get.o)"(.text.pthread_attr_getguardsize); /* size 0x10 */
	. = ALIGN(0x4); /* align for .text.pthread_attr_getinheritsched */
	"/home/rlyerly/Downloads/popcorn-compiler/test-install/aarch64/lib/libc.a(pthread_attr_get.o)"(.text.pthread_attr_getinheritsched); /* size 0x10 */
	. = ALIGN(0x4); /* align for .text.pthread_attr_getschedparam */
	"/home/rlyerly/Downloads/popcorn-compiler/test-install/aarch64/lib/libc.a(pthread_attr_get.o)"(.text.pthread_attr_getschedparam); /* size 0x10 */
	. = ALIGN(0x4); /* align for .text.pthread_attr_getschedpolicy */
	"/home/rlyerly/Downloads/popcorn-compiler/test-install/aarch64/lib/libc.a(pthread_attr_get.o)"(.text.pthread_attr_getschedpolicy); /* size 0x10 */
	. = ALIGN(0x4); /* align for .text.pthread_attr_getscope */
	"/home/rlyerly/Downloads/popcorn-compiler/test-install/aarch64/lib/libc.a(pthread_attr_get.o)"(.text.pthread_attr_getscope); /* size 0xc */
	. = ALIGN(0x4); /* align for .text.pthread_attr_getstack */
	"/home/rlyerly/Downloads/popcorn-compiler/test-install/aarch64/lib/libc.a(pthread_attr_get.o)"(.text.pthread_attr_getstack); /* size 0x2c */
	. = ALIGN(0x4); /* align for .text.pthread_attr_getstacksize */
	"/home/rlyerly/Downloads/popcorn-compiler/test-install/aarch64/lib/libc.a(pthread_attr_get.o)"(.text.pthread_attr_getstacksize); /* size 0x10 */
	. = ALIGN(0x4); /* align for .text.pthread_barrierattr_getpshared */
	"/home/rlyerly/Downloads/popcorn-compiler/test-install/aarch64/lib/libc.a(pthread_attr_get.o)"(.text.pthread_barrierattr_getpshared); /* size 0x18 */
	. = ALIGN(0x4); /* align for .text.pthread_condattr_getclock */
	"/home/rlyerly/Downloads/popcorn-compiler/test-install/aarch64/lib/libc.a(pthread_attr_get.o)"(.text.pthread_condattr_getclock); /* size 0x14 */
	. = ALIGN(0x4); /* align for .text.pthread_condattr_getpshared */
	"/home/rlyerly/Downloads/popcorn-compiler/test-install/aarch64/lib/libc.a(pthread_attr_get.o)"(.text.pthread_condattr_getpshared); /* size 0x14 */
	. = ALIGN(0x4); /* align for .text.pthread_mutexattr_getprotocol */
	"/home/rlyerly/Downloads/popcorn-compiler/test-install/aarch64/lib/libc.a(pthread_attr_get.o)"(.text.pthread_mutexattr_getprotocol); /* size 0xc */
	. = ALIGN(0x4); /* align for .text.pthread_mutexattr_getpshared */
	"/home/rlyerly/Downloads/popcorn-compiler/test-install/aarch64/lib/libc.a(pthread_attr_get.o)"(.text.pthread_mutexattr_getpshared); /* size 0x14 */
	. = ALIGN(0x4); /* align for .text.pthread_mutexattr_getrobust */
	"/home/rlyerly/Downloads/popcorn-compiler/test-install/aarch64/lib/libc.a(pthread_attr_get.o)"(.text.pthread_mutexattr_getrobust); /* size 0x14 */
	. = ALIGN(0x4); /* align for .text.pthread_mutexattr_gettype */
	"/home/rlyerly/Downloads/popcorn-compiler/test-install/aarch64/lib/libc.a(pthread_attr_get.o)"(.text.pthread_mutexattr_gettype); /* size 0x14 */
	. = ALIGN(0x4); /* align for .text.pthread_rwlockattr_getpshared */
	"/home/rlyerly/Downloads/popcorn-compiler/test-install/aarch64/lib/libc.a(pthread_attr_get.o)"(.text.pthread_rwlockattr_getpshared); /* size 0x10 */
	. = ALIGN(0x4); /* align for .text.pthread_getattr_np */
	"/home/rlyerly/Downloads/popcorn-compiler/test-install/aarch64/lib/libc.a(pthread_getattr_np.o)"(.text.pthread_getattr_np); /* size 0xec */
	. = ALIGN(0x4); /* align for .text.__pthread_getspecific */
	"/home/rlyerly/Downloads/popcorn-compiler/test-install/aarch64/lib/libc.a(pthread_getspecific.o)"(.text.__pthread_getspecific); /* size 0x10 */
	. = . + 0x7; /* padding after .text.__pthread_getspecific */
	. = ALIGN(0x4); /* align for .text.__pthread_key_create */
	"/home/rlyerly/Downloads/popcorn-compiler/test-install/aarch64/lib/libc.a(pthread_key_create.o)"(.text.__pthread_key_create); /* size 0x8c */
	. = ALIGN(0x4); /* align for .text.nodtor */
	"/home/rlyerly/Downloads/popcorn-compiler/test-install/aarch64/lib/libc.a(pthread_key_create.o)"(.text.nodtor); /* size 0x4 */
	. = ALIGN(0x4); /* align for .text.__pthread_key_delete */
	"/home/rlyerly/Downloads/popcorn-compiler/test-install/aarch64/lib/libc.a(pthread_key_create.o)"(.text.__pthread_key_delete); /* size 0x14 */
	. = ALIGN(0x4); /* align for .text.__pthread_tsd_run_dtors */
	"/home/rlyerly/Downloads/popcorn-compiler/test-install/aarch64/lib/libc.a(pthread_key_create.o)"(.text.__pthread_tsd_run_dtors); /* size 0x8c */
	. = ALIGN(0x4); /* align for .text.pthread_migrate_args */
	"/home/rlyerly/Downloads/popcorn-compiler/test-install/aarch64/lib/libc.a(pthread_migrate.o)"(.text.pthread_migrate_args); /* size 0xc */
	. = . + 0x4; /* padding after .text.pthread_migrate_args */
	. = ALIGN(0x4); /* align for .text.__pthread_self_internal */
	"/home/rlyerly/Downloads/popcorn-compiler/test-install/aarch64/lib/libc.a(pthread_self.o)"(.text.__pthread_self_internal); /* size 0x8 */
	. = . + 0x2; /* padding after .text.__pthread_self_internal */
	. = ALIGN(0x4); /* align for .text.pthread_setspecific */
	"/home/rlyerly/Downloads/popcorn-compiler/test-install/aarch64/lib/libc.a(pthread_setspecific.o)"(.text.pthread_setspecific); /* size 0x2c */
	. = ALIGN(0x4); /* align for .text.dummy */
	"/home/rlyerly/Downloads/popcorn-compiler/test-install/aarch64/lib/libc.a(close.o)"(.text.dummy); /* size 0x4 */
	. = ALIGN(0x4); /* align for .text.close */
	"/home/rlyerly/Downloads/popcorn-compiler/test-install/aarch64/lib/libc.a(close.o)"(.text.close); /* size 0x44 */
	. = ALIGN(0x4); /* align for .text.getpid */
	"/home/rlyerly/Downloads/popcorn-compiler/test-install/aarch64/lib/libc.a(getpid.o)"(.text.getpid); /* size 0xc */
	. = ALIGN(0x4); /* align for .text.lseek */
	"/home/rlyerly/Downloads/popcorn-compiler/test-install/aarch64/lib/libc.a(lseek.o)"(.text.lseek); /* size 0x14 */
	. = . + 0x1; /* padding after .text.lseek */
	. = ALIGN(0x4); /* align for .text.read */
	"/home/rlyerly/Downloads/popcorn-compiler/test-install/aarch64/lib/libc.a(read.o)"(.text.read); /* size 0x38 */
	. = ALIGN(0x4); /* align for .text.abort */
	"/home/rlyerly/Downloads/popcorn-compiler/test-install/aarch64/lib/libc.a(abort.o)"(.text.abort); /* size 0x34 */
	. = ALIGN(0x10); /* align for .text.__intscan */
	"/home/rlyerly/Downloads/popcorn-compiler/test-install/aarch64/lib/libc.a(intscan.o)"(.text.__intscan); /* size 0x5c8 */
	. = . + 0x6; /* padding after .text.__intscan */
	. = ALIGN(0x10); /* align for .text.__shlim */
	"/home/rlyerly/Downloads/popcorn-compiler/test-install/aarch64/lib/libc.a(shgetc.o)"(.text.__shlim); /* size 0x24 */
	. = . + 0x15; /* padding after .text.__shlim */
	. = ALIGN(0x10); /* align for .text.__shgetc */
	"/home/rlyerly/Downloads/popcorn-compiler/test-install/aarch64/lib/libc.a(shgetc.o)"(.text.__shgetc); /* size 0xbc */
	. = ALIGN(0x1); /* align for .text.__syscall */
	"/home/rlyerly/Downloads/popcorn-compiler/test-install/aarch64/lib/libc.a(syscall.o)"(.text.__syscall); /* size 0x28 */
	. = ALIGN(0x10); /* align for .text.__expand_heap */
	"/home/rlyerly/Downloads/popcorn-compiler/test-install/aarch64/lib/libc.a(expand_heap.o)"(.text.__expand_heap); /* size 0x178 */
	. = ALIGN(0x4); /* align for .text.__block_all_sigs */
	"/home/rlyerly/Downloads/popcorn-compiler/test-install/aarch64/lib/libc.a(block.o)"(.text.__block_all_sigs); /* size 0x24 */
	. = ALIGN(0x4); /* align for .text.__block_app_sigs */
	"/home/rlyerly/Downloads/popcorn-compiler/test-install/aarch64/lib/libc.a(block.o)"(.text.__block_app_sigs); /* size 0x24 */
	. = ALIGN(0x4); /* align for .text.__restore_sigs */
	"/home/rlyerly/Downloads/popcorn-compiler/test-install/aarch64/lib/libc.a(block.o)"(.text.__restore_sigs); /* size 0x20 */
	. = ALIGN(0x4); /* align for .text.raise */
	"/home/rlyerly/Downloads/popcorn-compiler/test-install/aarch64/lib/libc.a(raise.o)"(.text.raise); /* size 0x58 */
	. = ALIGN(0x4); /* align for .text.__fdopen */
	"/home/rlyerly/Downloads/popcorn-compiler/test-install/aarch64/lib/libc.a(__fdopen.o)"(.text.__fdopen); /* size 0x1b4 */
	. = ALIGN(0x4); /* align for .text.__fmodeflags */
	"/home/rlyerly/Downloads/popcorn-compiler/test-install/aarch64/lib/libc.a(__fmodeflags.o)"(.text.__fmodeflags); /* size 0x94 */
	. = ALIGN(0x4); /* align for .text.__stdio_read */
	"/home/rlyerly/Downloads/popcorn-compiler/test-install/aarch64/lib/libc.a(__stdio_read.o)"(.text.__stdio_read); /* size 0xe0 */
	. = ALIGN(0x4); /* align for .text.__string_read */
	"/home/rlyerly/Downloads/popcorn-compiler/test-install/aarch64/lib/libc.a(__string_read.o)"(.text.__string_read); /* size 0x84 */
	. = ALIGN(0x4); /* align for .text.__uflow */
	"/home/rlyerly/Downloads/popcorn-compiler/test-install/aarch64/lib/libc.a(__uflow.o)"(.text.__uflow); /* size 0x54 */
	. = ALIGN(0x4); /* align for .text.fputc */
	"/home/rlyerly/Downloads/popcorn-compiler/test-install/aarch64/lib/libc.a(fputc.o)"(.text.fputc); /* size 0xd4 */
	. = ALIGN(0x4); /* align for .text.getdelim */
	"/home/rlyerly/Downloads/popcorn-compiler/test-install/aarch64/lib/libc.a(getdelim.o)"(.text.getdelim); /* size 0x270 */
	. = ALIGN(0x4); /* align for .text.__ofl_add */
	"/home/rlyerly/Downloads/popcorn-compiler/test-install/aarch64/lib/libc.a(ofl_add.o)"(.text.__ofl_add); /* size 0x40 */
	. = ALIGN(0x4); /* align for .text.vfscanf */
	"/home/rlyerly/Downloads/popcorn-compiler/test-install/aarch64/lib/libc.a(vfscanf.o)"(.text.vfscanf); /* size 0xd98 */
	. = ALIGN(0x10); /* align for .text.__floatscan */
	"/home/rlyerly/Downloads/popcorn-compiler/test-install/aarch64/lib/libc.a(floatscan.o)"(.text.__floatscan); /* size 0x163c */
	. = . + 0xe0; /* padding after .text.__floatscan */
	. = ALIGN(0x10); /* align for .text.scanexp */
	"/home/rlyerly/Downloads/popcorn-compiler/test-install/aarch64/lib/libc.a(floatscan.o)"(.text.scanexp); /* size 0x21c */
	. = ALIGN(0x4); /* align for .text.copysignl */
	"/home/rlyerly/Downloads/popcorn-compiler/test-install/aarch64/lib/libc.a(copysignl.o)"(.text.copysignl); /* size 0x2c */
	. = ALIGN(0x4); /* align for .text.scalbn */
	"/home/rlyerly/Downloads/popcorn-compiler/test-install/aarch64/lib/libc.a(scalbn.o)"(.text.scalbn); /* size 0x88 */
	. = . + 0x6; /* padding after .text.scalbn */
	. = ALIGN(0x4); /* align for .text.scalbnl */
	"/home/rlyerly/Downloads/popcorn-compiler/test-install/aarch64/lib/libc.a(scalbnl.o)"(.text.scalbnl); /* size 0xd4 */
	. = ALIGN(0x4); /* align for .text.fabs */
	"/home/rlyerly/Downloads/popcorn-compiler/test-install/aarch64/lib/libc.a(fabs.o)"(.text.fabs); /* size 0x8 */
	. = . + 0xa; /* padding after .text.fabs */
	. = ALIGN(0x4); /* align for .text.fmodl */
	"/home/rlyerly/Downloads/popcorn-compiler/test-install/aarch64/lib/libc.a(fmodl.o)"(.text.fmodl); /* size 0x254 */
	. = ALIGN(0x4); /* align for .text.mbrtowc */
	"/home/rlyerly/Downloads/popcorn-compiler/test-install/aarch64/lib/libc.a(mbrtowc.o)"(.text.mbrtowc); /* size 0x144 */
	. = ALIGN(0x4); /* align for .text.mbsinit */
	"/home/rlyerly/Downloads/popcorn-compiler/test-install/aarch64/lib/libc.a(mbsinit.o)"(.text.mbsinit); /* size 0x1c */
	. = ALIGN(0x4); /* align for .text.__overflow */
	"/home/rlyerly/Downloads/popcorn-compiler/test-install/aarch64/lib/libc.a(__overflow.o)"(.text.__overflow); /* size 0xa8 */
	. = ALIGN(0x4); /* align for .text.__toread */
	"/home/rlyerly/Downloads/popcorn-compiler/test-install/aarch64/lib/libc.a(__toread.o)"(.text.__toread); /* size 0x88 */
	. = ALIGN(0x4); /* align for .text.__toread_needs_stdio_exit */
	"/home/rlyerly/Downloads/popcorn-compiler/test-install/aarch64/lib/libc.a(__toread.o)"(.text.__toread_needs_stdio_exit); /* size 0x4 */
	. = . + 0x1; /* padding after .text.__toread_needs_stdio_exit */
	. = ALIGN(0x1); /* align for .text.__restore */
	"/home/rlyerly/Downloads/popcorn-compiler/test-install/aarch64/lib/libc.a(restore.o)"(.text.__restore); /* size 0x8 */
	*(.text);
}
  
  .fini           :
  {
/*    KEEP (*(SORT_NONE(.fini)))*/
  } =0
  PROVIDE (__etext = .);
  PROVIDE (_etext = .);
  PROVIDE (etext = .);
  
.rodata	: ALIGN(0x100000)
{
	. = . + 1;
	. = ALIGN(0x1000);
	. = ALIGN(0x1); /* align for .rodata.fizzbuzz__str_fizzbuzz__ */
	"fizzbuzz.o"(.rodata.fizzbuzz__str_fizzbuzz__); /* size 0xa */
	. = ALIGN(0x1); /* align for .rodata.fizzbuzz__str_1_fizz__ */
	"fizzbuzz.o"(.rodata.fizzbuzz__str_1_fizz__); /* size 0x6 */
	. = ALIGN(0x1); /* align for .rodata.fizzbuzz__str_2_buzz__ */
	"fizzbuzz.o"(.rodata.fizzbuzz__str_2_buzz__); /* size 0x6 */
	. = ALIGN(0x1); /* align for .rodata.src_env___libc_start_main__str_ */
	"/home/rlyerly/Downloads/popcorn-compiler/test-install/aarch64/lib/libc.a(__libc_start_main.o)"(.rodata.src_env___libc_start_main__str_); /* size 0x1 */
	. = ALIGN(0x10); /* align for .rodata.src_env___libc_start_main___init_libc_pfd_ */
	"/home/rlyerly/Downloads/popcorn-compiler/test-install/aarch64/lib/libc.a(__libc_start_main.o)"(.rodata.src_env___libc_start_main___init_libc_pfd_); /* size 0x18 */
	. = ALIGN(0x1); /* align for .rodata.src_env___libc_start_main__str_1__dev_null_ */
	"/home/rlyerly/Downloads/popcorn-compiler/test-install/aarch64/lib/libc.a(__libc_start_main.o)"(.rodata.src_env___libc_start_main__str_1__dev_null_); /* size 0xa */
	. = ALIGN(0x8); /* align for .rodata.stdout */
	"/home/rlyerly/Downloads/popcorn-compiler/test-install/aarch64/lib/libc.a(stdout.o)"(.rodata.stdout); /* size 0x8 */
	. = ALIGN(0x8); /* align for .rodata.printf_core */
	"/home/rlyerly/Downloads/popcorn-compiler/test-install/aarch64/lib/libc.a(vfprintf.o)"(.rodata.printf_core); /* size 0x170 */
	. = ALIGN(0x8); /* align for .rodata.pop_arg */
	"/home/rlyerly/Downloads/popcorn-compiler/test-install/aarch64/lib/libc.a(vfprintf.o)"(.rodata.pop_arg); /* size 0x90 */
	. = ALIGN(0x10); /* align for .rodata.src_stdio_vfprintf_c_states */
	"/home/rlyerly/Downloads/popcorn-compiler/test-install/aarch64/lib/libc.a(vfprintf.o)"(.rodata.src_stdio_vfprintf_c_states); /* size 0x1d0 */
	. = ALIGN(0x1); /* align for .rodata.src_stdio_vfprintf__str______0X0x_ */
	"/home/rlyerly/Downloads/popcorn-compiler/test-install/aarch64/lib/libc.a(vfprintf.o)"(.rodata.src_stdio_vfprintf__str______0X0x_); /* size 0xa */
	. = ALIGN(0x1); /* align for .rodata.src_stdio_vfprintf__str_1__null__ */
	"/home/rlyerly/Downloads/popcorn-compiler/test-install/aarch64/lib/libc.a(vfprintf.o)"(.rodata.src_stdio_vfprintf__str_1__null__); /* size 0x7 */
	. = ALIGN(0x10); /* align for .rodata.src_stdio_vfprintf_c_xdigits */
	"/home/rlyerly/Downloads/popcorn-compiler/test-install/aarch64/lib/libc.a(vfprintf.o)"(.rodata.src_stdio_vfprintf_c_xdigits); /* size 0x10 */
	. = ALIGN(0x1); /* align for .rodata.src_stdio_vfprintf__str_2__0X_0X_0X_ */
	"/home/rlyerly/Downloads/popcorn-compiler/test-install/aarch64/lib/libc.a(vfprintf.o)"(.rodata.src_stdio_vfprintf__str_2__0X_0X_0X_); /* size 0x13 */
	. = ALIGN(0x1); /* align for .rodata.src_stdio_vfprintf__str_3_inf_ */
	"/home/rlyerly/Downloads/popcorn-compiler/test-install/aarch64/lib/libc.a(vfprintf.o)"(.rodata.src_stdio_vfprintf__str_3_inf_); /* size 0x4 */
	. = ALIGN(0x1); /* align for .rodata.src_stdio_vfprintf__str_4_INF_ */
	"/home/rlyerly/Downloads/popcorn-compiler/test-install/aarch64/lib/libc.a(vfprintf.o)"(.rodata.src_stdio_vfprintf__str_4_INF_); /* size 0x4 */
	. = ALIGN(0x1); /* align for .rodata.src_stdio_vfprintf__str_5_nan_ */
	"/home/rlyerly/Downloads/popcorn-compiler/test-install/aarch64/lib/libc.a(vfprintf.o)"(.rodata.src_stdio_vfprintf__str_5_nan_); /* size 0x4 */
	. = ALIGN(0x1); /* align for .rodata.src_stdio_vfprintf__str_6_NAN_ */
	"/home/rlyerly/Downloads/popcorn-compiler/test-install/aarch64/lib/libc.a(vfprintf.o)"(.rodata.src_stdio_vfprintf__str_6_NAN_); /* size 0x4 */
	. = ALIGN(0x1); /* align for .rodata.src_stdio_vfprintf__str_7___ */
	"/home/rlyerly/Downloads/popcorn-compiler/test-install/aarch64/lib/libc.a(vfprintf.o)"(.rodata.src_stdio_vfprintf__str_7___); /* size 0x2 */
	. = ALIGN(0x10); /* align for .rodata.src_errno_strerror_c_errid */
	"/home/rlyerly/Downloads/popcorn-compiler/test-install/aarch64/lib/libc.a(strerror.o)"(.rodata.src_errno_strerror_c_errid); /* size 0x58 */
	. = ALIGN(0x10); /* align for .rodata.src_errno_strerror_c_errmsg */
	"/home/rlyerly/Downloads/popcorn-compiler/test-install/aarch64/lib/libc.a(strerror.o)"(.rodata.src_errno_strerror_c_errmsg); /* size 0x70c */
	. = ALIGN(0x1); /* align for .rodata.src_migrate__str_0_____Unsu */
	"/home/rlyerly/Downloads/popcorn-compiler/test-install/aarch64/lib/libmigrate.a(migrate.o)"(.rodata.src_migrate__str_0_____Unsu); /* size 0x21 */
	. = ALIGN(0x1); /* align for .rodata.src_migrate__str_1_src_migrat */
	"/home/rlyerly/Downloads/popcorn-compiler/test-install/aarch64/lib/libmigrate.a(migrate.o)"(.rodata.src_migrate__str_1_src_migrat); /* size 0xe */
	. = ALIGN(0x1); /* align for .rodata.src_migrate___func_____migrate_shim_internal___migrate_ */
	"/home/rlyerly/Downloads/popcorn-compiler/test-install/aarch64/lib/libmigrate.a(migrate.o)"(.rodata.src_migrate___func_____migrate_shim_internal___migrate_); /* size 0x18 */
	. = ALIGN(0x1); /* align for .rodata.src_migrate__str_2_0_____Coul */
	"/home/rlyerly/Downloads/popcorn-compiler/test-install/aarch64/lib/libmigrate.a(migrate.o)"(.rodata.src_migrate__str_2_0_____Coul); /* size 0x19 */
	. = ALIGN(0x1); /* align for .rodata.src_trigger__str_Number_of_ */
	"/home/rlyerly/Downloads/popcorn-compiler/test-install/aarch64/lib/libmigrate.a(trigger.o)"(.rodata.src_trigger__str_Number_of_); /* size 0x33 */
	. = ALIGN(0x1); /* align for .rodata.src_trigger__str_1____llu_ns_ */
	"/home/rlyerly/Downloads/popcorn-compiler/test-install/aarch64/lib/libmigrate.a(trigger.o)"(.rodata.src_trigger__str_1____llu_ns_); /* size 0xb */
	. = ALIGN(0x1); /* align for .rodata.src_trigger__str_2_No_startin */
	"/home/rlyerly/Downloads/popcorn-compiler/test-install/aarch64/lib/libmigrate.a(trigger.o)"(.rodata.src_trigger__str_2_No_startin); /* size 0x17 */
	. = ALIGN(0x1); /* align for .rodata.src_trigger__str_3_start____U */
	"/home/rlyerly/Downloads/popcorn-compiler/test-install/aarch64/lib/libmigrate.a(trigger.o)"(.rodata.src_trigger__str_3_start____U); /* size 0x30 */
	. = ALIGN(0x1); /* align for .rodata.src_trigger__str_4_src_trigge */
	"/home/rlyerly/Downloads/popcorn-compiler/test-install/aarch64/lib/libmigrate.a(trigger.o)"(.rodata.src_trigger__str_4_src_trigge); /* size 0xe */
	. = ALIGN(0x1); /* align for .rodata.src_trigger___func___clear_migrate_flag_clear_migr */
	"/home/rlyerly/Downloads/popcorn-compiler/test-install/aarch64/lib/libmigrate.a(trigger.o)"(.rodata.src_trigger___func___clear_migrate_flag_clear_migr); /* size 0x13 */
	. = ALIGN(0x1); /* align for .rodata.src_trigger__str_5_WARNING__t */
	"/home/rlyerly/Downloads/popcorn-compiler/test-install/aarch64/lib/libmigrate.a(trigger.o)"(.rodata.src_trigger__str_5_WARNING__t); /* size 0x25 */
	. = ALIGN(0x1); /* align for .rodata.src_trigger__str_6_Could_not_ */
	"/home/rlyerly/Downloads/popcorn-compiler/test-install/aarch64/lib/libmigrate.a(trigger.o)"(.rodata.src_trigger__str_6_Could_not_); /* size 0x39 */
	. = ALIGN(0x1); /* align for .rodata.src_trigger__str_7_Could_not_ */
	"/home/rlyerly/Downloads/popcorn-compiler/test-install/aarch64/lib/libmigrate.a(trigger.o)"(.rodata.src_trigger__str_7_Could_not_); /* size 0x34 */
	. = ALIGN(0x1); /* align for .rodata.src_trigger__str_8_Could_not_ */
	"/home/rlyerly/Downloads/popcorn-compiler/test-install/aarch64/lib/libmigrate.a(trigger.o)"(.rodata.src_trigger__str_8_Could_not_); /* size 0x3b */
	. = ALIGN(0x1); /* align for .rodata.src_userspace__str_ST_AARCH64 */
	"/home/rlyerly/Downloads/popcorn-compiler/test-install/aarch64/lib/libstack-transform.a(userspace.o)"(.rodata.src_userspace__str_ST_AARCH64); /* size 0xf */
	. = ALIGN(0x1); /* align for .rodata.src_userspace__str_1__s_aarch64 */
	"/home/rlyerly/Downloads/popcorn-compiler/test-install/aarch64/lib/libstack-transform.a(userspace.o)"(.rodata.src_userspace__str_1__s_aarch64); /* size 0xb */
	. = ALIGN(0x1); /* align for .rodata.src_userspace__str_2_ST_POWERPC */
	"/home/rlyerly/Downloads/popcorn-compiler/test-install/aarch64/lib/libstack-transform.a(userspace.o)"(.rodata.src_userspace__str_2_ST_POWERPC); /* size 0x11 */
	. = ALIGN(0x1); /* align for .rodata.src_userspace__str_3__s_powerpc */
	"/home/rlyerly/Downloads/popcorn-compiler/test-install/aarch64/lib/libstack-transform.a(userspace.o)"(.rodata.src_userspace__str_3__s_powerpc); /* size 0xd */
	. = ALIGN(0x1); /* align for .rodata.src_userspace__str_4_ST_X86_64_ */
	"/home/rlyerly/Downloads/popcorn-compiler/test-install/aarch64/lib/libstack-transform.a(userspace.o)"(.rodata.src_userspace__str_4_ST_X86_64_); /* size 0xe */
	. = ALIGN(0x1); /* align for .rodata.src_userspace__str_5__s_x86_64_ */
	"/home/rlyerly/Downloads/popcorn-compiler/test-install/aarch64/lib/libstack-transform.a(userspace.o)"(.rodata.src_userspace__str_5__s_x86_64_); /* size 0xa */
	. = ALIGN(0x1); /* align for .rodata.src_userspace__str_6__proc__d_m */
	"/home/rlyerly/Downloads/popcorn-compiler/test-install/aarch64/lib/libstack-transform.a(userspace.o)"(.rodata.src_userspace__str_6__proc__d_m); /* size 0xe */
	. = ALIGN(0x1); /* align for .rodata.src_userspace__str_7_r_ */
	"/home/rlyerly/Downloads/popcorn-compiler/test-install/aarch64/lib/libstack-transform.a(userspace.o)"(.rodata.src_userspace__str_7_r_); /* size 0x2 */
	. = ALIGN(0x1); /* align for .rodata.src_userspace__str_8__lx__lx__s */
	"/home/rlyerly/Downloads/popcorn-compiler/test-install/aarch64/lib/libstack-transform.a(userspace.o)"(.rodata.src_userspace__str_8__lx__lx__s); /* size 0x19 */
	. = ALIGN(0x1); /* align for .rodata.src_userspace__str_9__stack__ */
	"/home/rlyerly/Downloads/popcorn-compiler/test-install/aarch64/lib/libstack-transform.a(userspace.o)"(.rodata.src_userspace__str_9__stack__); /* size 0x8 */
	. = ALIGN(0x1); /* align for .rodata.src_init__str__stack_tra */
	"/home/rlyerly/Downloads/popcorn-compiler/test-install/aarch64/lib/libstack-transform.a(init.o)"(.rodata.src_init__str__stack_tra); /* size 0x1f */
	. = ALIGN(0x1); /* align for .rodata.src_init__str_1__stack_tra */
	"/home/rlyerly/Downloads/popcorn-compiler/test-install/aarch64/lib/libstack-transform.a(init.o)"(.rodata.src_init__str_1__stack_tra); /* size 0x18 */
	. = ALIGN(0x1); /* align for .rodata.src_init__str_2__stack_tra */
	"/home/rlyerly/Downloads/popcorn-compiler/test-install/aarch64/lib/libstack-transform.a(init.o)"(.rodata.src_init__str_2__stack_tra); /* size 0x14 */
	. = ALIGN(0x1); /* align for .rodata.src_init__str_3__stack_tra */
	"/home/rlyerly/Downloads/popcorn-compiler/test-install/aarch64/lib/libstack-transform.a(init.o)"(.rodata.src_init__str_3__stack_tra); /* size 0x16 */
	. = ALIGN(0x1); /* align for .rodata.src_init__str_4__stack_tra */
	"/home/rlyerly/Downloads/popcorn-compiler/test-install/aarch64/lib/libstack-transform.a(init.o)"(.rodata.src_init__str_4__stack_tra); /* size 0x16 */
	. = ALIGN(0x1); /* align for .rodata.src_init__str_5__stack_tra */
	"/home/rlyerly/Downloads/popcorn-compiler/test-install/aarch64/lib/libstack-transform.a(init.o)"(.rodata.src_init__str_5__stack_tra); /* size 0x1c */
	. = ALIGN(0x1); /* align for .rodata.src_rewrite__str__src_rewri */
	"/home/rlyerly/Downloads/popcorn-compiler/test-install/aarch64/lib/libstack-transform.a(rewrite.o)"(.rodata.src_rewrite__str__src_rewri); /* size 0x42 */
	. = ALIGN(0x1); /* align for .rodata.src_rewrite__str_1__src_rewri */
	"/home/rlyerly/Downloads/popcorn-compiler/test-install/aarch64/lib/libstack-transform.a(rewrite.o)"(.rodata.src_rewrite__str_1__src_rewri); /* size 0x68 */
	. = ALIGN(0x1); /* align for .rodata.src_rewrite__str_2__src_rewri */
	"/home/rlyerly/Downloads/popcorn-compiler/test-install/aarch64/lib/libstack-transform.a(rewrite.o)"(.rodata.src_rewrite__str_2__src_rewri); /* size 0x54 */
	. = ALIGN(0x1); /* align for .rodata.src_rewrite__str_3__src_rewri */
	"/home/rlyerly/Downloads/popcorn-compiler/test-install/aarch64/lib/libstack-transform.a(rewrite.o)"(.rodata.src_rewrite__str_3__src_rewri); /* size 0x61 */
	. = ALIGN(0x1); /* align for .rodata.src_util__str_aarch64_ */
	"/home/rlyerly/Downloads/popcorn-compiler/test-install/aarch64/lib/libstack-transform.a(util.o)"(.rodata.src_util__str_aarch64_); /* size 0x8 */
	. = ALIGN(0x1); /* align for .rodata.src_util__str_1_powerpc64_ */
	"/home/rlyerly/Downloads/popcorn-compiler/test-install/aarch64/lib/libstack-transform.a(util.o)"(.rodata.src_util__str_1_powerpc64_); /* size 0xa */
	. = ALIGN(0x1); /* align for .rodata.src_util__str_2_x86_64_ */
	"/home/rlyerly/Downloads/popcorn-compiler/test-install/aarch64/lib/libstack-transform.a(util.o)"(.rodata.src_util__str_2_x86_64_); /* size 0x7 */
	. = ALIGN(0x1); /* align for .rodata.src_util__str_3_unknown_un */
	"/home/rlyerly/Downloads/popcorn-compiler/test-install/aarch64/lib/libstack-transform.a(util.o)"(.rodata.src_util__str_3_unknown_un); /* size 0x21 */
	. = ALIGN(0x10); /* align for .rodata.src_arch_aarch64_properties_c_callee_saved_aarch64 */
	"/home/rlyerly/Downloads/popcorn-compiler/test-install/aarch64/lib/libstack-transform.a(properties.o)"(.rodata.src_arch_aarch64_properties_c_callee_saved_aarch64); /* size 0x28 */
	. = ALIGN(0x10); /* align for .rodata.src_arch_aarch64_properties_c_callee_saved_size_aarch64 */
	"/home/rlyerly/Downloads/popcorn-compiler/test-install/aarch64/lib/libstack-transform.a(properties.o)"(.rodata.src_arch_aarch64_properties_c_callee_saved_size_aarch64); /* size 0x28 */
	. = ALIGN(0x8); /* align for .rodata.properties_aarch64 */
	"/home/rlyerly/Downloads/popcorn-compiler/test-install/aarch64/lib/libstack-transform.a(properties.o)"(.rodata.properties_aarch64); /* size 0x38 */
	. = ALIGN(0x1); /* align for .rodata.src_arch_aarch64_properties__str__src_arch_ */
	"/home/rlyerly/Downloads/popcorn-compiler/test-install/aarch64/lib/libstack-transform.a(properties.o)"(.rodata.src_arch_aarch64_properties__str__src_arch_); /* size 0x51 */
	. = ALIGN(0x8); /* align for .rodata.reg_aarch64 */
	"/home/rlyerly/Downloads/popcorn-compiler/test-install/aarch64/lib/libstack-transform.a(regs.o)"(.rodata.reg_aarch64); /* size 0x300 */
	. = ALIGN(0x8); /* align for .rodata.regs_aarch64 */
	"/home/rlyerly/Downloads/popcorn-compiler/test-install/aarch64/lib/libstack-transform.a(regs.o)"(.rodata.regs_aarch64); /* size 0xa8 */
	. = ALIGN(0x1); /* align for .rodata.src_arch_aarch64_regs__str__src_arch_ */
	"/home/rlyerly/Downloads/popcorn-compiler/test-install/aarch64/lib/libstack-transform.a(regs.o)"(.rodata.src_arch_aarch64_regs__str__src_arch_); /* size 0x4c */
	. = ALIGN(0x1); /* align for .rodata.src_arch_aarch64_regs__str_1__src_arch_ */
	"/home/rlyerly/Downloads/popcorn-compiler/test-install/aarch64/lib/libstack-transform.a(regs.o)"(.rodata.src_arch_aarch64_regs__str_1__src_arch_); /* size 0x4c */
	. = ALIGN(0x8); /* align for .rodata.is_callee_saved_powerpc64 */
	"/home/rlyerly/Downloads/popcorn-compiler/test-install/aarch64/lib/libstack-transform.a(properties.o)"(.rodata.is_callee_saved_powerpc64); /* size 0x208 */
	. = ALIGN(0x8); /* align for .rodata.callee_reg_size_powerpc64 */
	"/home/rlyerly/Downloads/popcorn-compiler/test-install/aarch64/lib/libstack-transform.a(properties.o)"(.rodata.callee_reg_size_powerpc64); /* size 0x210 */
	. = ALIGN(0x10); /* align for .rodata.src_arch_powerpc64_properties_c_callee_saved_powerpc64 */
	"/home/rlyerly/Downloads/popcorn-compiler/test-install/aarch64/lib/libstack-transform.a(properties.o)"(.rodata.src_arch_powerpc64_properties_c_callee_saved_powerpc64); /* size 0x4e */
	. = ALIGN(0x10); /* align for .rodata.src_arch_powerpc64_properties_c_callee_saved_size_powerpc64 */
	"/home/rlyerly/Downloads/popcorn-compiler/test-install/aarch64/lib/libstack-transform.a(properties.o)"(.rodata.src_arch_powerpc64_properties_c_callee_saved_size_powerpc64); /* size 0x4c */
	. = ALIGN(0x8); /* align for .rodata.properties_powerpc64 */
	"/home/rlyerly/Downloads/popcorn-compiler/test-install/aarch64/lib/libstack-transform.a(properties.o)"(.rodata.properties_powerpc64); /* size 0x38 */
	. = ALIGN(0x1); /* align for .rodata.src_arch_powerpc64_properties__str__src_arch_ */
	"/home/rlyerly/Downloads/popcorn-compiler/test-install/aarch64/lib/libstack-transform.a(properties.o)"(.rodata.src_arch_powerpc64_properties__str__src_arch_); /* size 0x56 */
	. = ALIGN(0x8); /* align for .rodata.reg_powerpc64 */
	"/home/rlyerly/Downloads/popcorn-compiler/test-install/aarch64/lib/libstack-transform.a(regs.o)"(.rodata.reg_powerpc64); /* size 0x218 */
	. = ALIGN(0x8); /* align for .rodata.regs_powerpc64 */
	"/home/rlyerly/Downloads/popcorn-compiler/test-install/aarch64/lib/libstack-transform.a(regs.o)"(.rodata.regs_powerpc64); /* size 0xa8 */
	. = ALIGN(0x1); /* align for .rodata.src_arch_powerpc64_regs__str__src_arch_ */
	"/home/rlyerly/Downloads/popcorn-compiler/test-install/aarch64/lib/libstack-transform.a(regs.o)"(.rodata.src_arch_powerpc64_regs__str__src_arch_); /* size 0x50 */
	. = ALIGN(0x1); /* align for .rodata.src_arch_powerpc64_regs__str_1__src_arch_ */
	"/home/rlyerly/Downloads/popcorn-compiler/test-install/aarch64/lib/libstack-transform.a(regs.o)"(.rodata.src_arch_powerpc64_regs__str_1__src_arch_); /* size 0x50 */
	. = ALIGN(0x2); /* align for .rodata.src_arch_x86_64_properties_c_callee_saved_x86_64 */
	"/home/rlyerly/Downloads/popcorn-compiler/test-install/aarch64/lib/libstack-transform.a(properties.o)"(.rodata.src_arch_x86_64_properties_c_callee_saved_x86_64); /* size 0xe */
	. = ALIGN(0x2); /* align for .rodata.src_arch_x86_64_properties_c_callee_saved_size_x86_64 */
	"/home/rlyerly/Downloads/popcorn-compiler/test-install/aarch64/lib/libstack-transform.a(properties.o)"(.rodata.src_arch_x86_64_properties_c_callee_saved_size_x86_64); /* size 0xe */
	. = ALIGN(0x8); /* align for .rodata.properties_x86_64 */
	"/home/rlyerly/Downloads/popcorn-compiler/test-install/aarch64/lib/libstack-transform.a(properties.o)"(.rodata.properties_x86_64); /* size 0x38 */
	. = ALIGN(0x1); /* align for .rodata.src_arch_x86_64_properties__str__src_arch_ */
	"/home/rlyerly/Downloads/popcorn-compiler/test-install/aarch64/lib/libstack-transform.a(properties.o)"(.rodata.src_arch_x86_64_properties__str__src_arch_); /* size 0x4f */
	. = ALIGN(0x8); /* align for .rodata.reg_x86_64 */
	"/home/rlyerly/Downloads/popcorn-compiler/test-install/aarch64/lib/libstack-transform.a(regs.o)"(.rodata.reg_x86_64); /* size 0x108 */
	. = ALIGN(0x8); /* align for .rodata.regs_x86_64 */
	"/home/rlyerly/Downloads/popcorn-compiler/test-install/aarch64/lib/libstack-transform.a(regs.o)"(.rodata.regs_x86_64); /* size 0xa8 */
	. = ALIGN(0x1); /* align for .rodata.src_arch_x86_64_regs__str__src_arch_ */
	"/home/rlyerly/Downloads/popcorn-compiler/test-install/aarch64/lib/libstack-transform.a(regs.o)"(.rodata.src_arch_x86_64_regs__str__src_arch_); /* size 0x4b */
	. = ALIGN(0x1); /* align for .rodata.src_arch_x86_64_regs__str_1__src_arch_ */
	"/home/rlyerly/Downloads/popcorn-compiler/test-install/aarch64/lib/libstack-transform.a(regs.o)"(.rodata.src_arch_x86_64_regs__str_1__src_arch_); /* size 0x4b */
	. = ALIGN(0x1); /* align for .rodata.src_arch_x86_64_regs__str_2__src_arch_ */
	"/home/rlyerly/Downloads/popcorn-compiler/test-install/aarch64/lib/libstack-transform.a(regs.o)"(.rodata.src_arch_x86_64_regs__str_2__src_arch_); /* size 0x4a */
	. = ALIGN(0x1); /* align for .rodata.src_arch_x86_64_regs__str_3__src_arch_ */
	"/home/rlyerly/Downloads/popcorn-compiler/test-install/aarch64/lib/libstack-transform.a(regs.o)"(.rodata.src_arch_x86_64_regs__str_3__src_arch_); /* size 0x4a */
	. = ALIGN(0x8); /* align for .rodata.put_val_arch */
	"/home/rlyerly/Downloads/popcorn-compiler/test-install/aarch64/lib/libstack-transform.a(data.o)"(.rodata.put_val_arch); /* size 0xb0 */
	. = ALIGN(0x8); /* align for .rodata.points_to_stack */
	"/home/rlyerly/Downloads/popcorn-compiler/test-install/aarch64/lib/libstack-transform.a(data.o)"(.rodata.points_to_stack); /* size 0x28 */
	. = ALIGN(0x1); /* align for .rodata.src_data__str__src_data_ */
	"/home/rlyerly/Downloads/popcorn-compiler/test-install/aarch64/lib/libstack-transform.a(data.o)"(.rodata.src_data__str__src_data_); /* size 0x38 */
	. = ALIGN(0x1); /* align for .rodata.src_data__str_1__src_data_ */
	"/home/rlyerly/Downloads/popcorn-compiler/test-install/aarch64/lib/libstack-transform.a(data.o)"(.rodata.src_data__str_1__src_data_); /* size 0x50 */
	. = ALIGN(0x1); /* align for .rodata.src_data__str_2__src_data_ */
	"/home/rlyerly/Downloads/popcorn-compiler/test-install/aarch64/lib/libstack-transform.a(data.o)"(.rodata.src_data__str_2__src_data_); /* size 0x3d */
	. = ALIGN(0x1); /* align for .rodata.src_data__str_3__src_data_ */
	"/home/rlyerly/Downloads/popcorn-compiler/test-install/aarch64/lib/libstack-transform.a(data.o)"(.rodata.src_data__str_3__src_data_); /* size 0x30 */
	. = ALIGN(0x1); /* align for .rodata.src_data__str_4__src_data_ */
	"/home/rlyerly/Downloads/popcorn-compiler/test-install/aarch64/lib/libstack-transform.a(data.o)"(.rodata.src_data__str_4__src_data_); /* size 0x49 */
	. = ALIGN(0x1); /* align for .rodata.src_data__str_5__src_data_ */
	"/home/rlyerly/Downloads/popcorn-compiler/test-install/aarch64/lib/libstack-transform.a(data.o)"(.rodata.src_data__str_5__src_data_); /* size 0x3f */
	. = ALIGN(0x1); /* align for .rodata.src_data__str_6__src_data_ */
	"/home/rlyerly/Downloads/popcorn-compiler/test-install/aarch64/lib/libstack-transform.a(data.o)"(.rodata.src_data__str_6__src_data_); /* size 0x3d */
	. = ALIGN(0x1); /* align for .rodata.src_data__str_7__src_data_ */
	"/home/rlyerly/Downloads/popcorn-compiler/test-install/aarch64/lib/libstack-transform.a(data.o)"(.rodata.src_data__str_7__src_data_); /* size 0x3f */
	. = ALIGN(0x1); /* align for .rodata.src_data__str_8__src_data_ */
	"/home/rlyerly/Downloads/popcorn-compiler/test-install/aarch64/lib/libstack-transform.a(data.o)"(.rodata.src_data__str_8__src_data_); /* size 0x37 */
	. = ALIGN(0x1); /* align for .rodata.src_data__str_9__src_data_ */
	"/home/rlyerly/Downloads/popcorn-compiler/test-install/aarch64/lib/libstack-transform.a(data.o)"(.rodata.src_data__str_9__src_data_); /* size 0x37 */
	. = ALIGN(0x1); /* align for .rodata.src_data__str_10__src_data_ */
	"/home/rlyerly/Downloads/popcorn-compiler/test-install/aarch64/lib/libstack-transform.a(data.o)"(.rodata.src_data__str_10__src_data_); /* size 0x3f */
	. = ALIGN(0x1); /* align for .rodata.src_data__str_11__src_data_ */
	"/home/rlyerly/Downloads/popcorn-compiler/test-install/aarch64/lib/libstack-transform.a(data.o)"(.rodata.src_data__str_11__src_data_); /* size 0x3f */
	. = ALIGN(0x10); /* align for .rodata.fmag */
	"/home/rlyerly/Downloads/popcorn-compiler/test-install/aarch64/lib/libelf.a(begin.o)"(.rodata.fmag); /* size 0x3 */
	. = ALIGN(0x20); /* align for .rodata.xlate32_11 */
	"/home/rlyerly/Downloads/popcorn-compiler/test-install/aarch64/lib/libelf.a(32.xlatetof.o)"(.rodata.xlate32_11); /* size 0x220 */
	. = ALIGN(0x20); /* align for .rodata._elf_data_init */
	"/home/rlyerly/Downloads/popcorn-compiler/test-install/aarch64/lib/libelf.a(cook.o)"(.rodata._elf_data_init); /* size 0x58 */
	. = ALIGN(0x20); /* align for .rodata._elf_scn_init */
	"/home/rlyerly/Downloads/popcorn-compiler/test-install/aarch64/lib/libelf.a(cook.o)"(.rodata._elf_scn_init); /* size 0xa0 */
	. = ALIGN(0x20); /* align for .rodata.xlate64_11 */
	"/home/rlyerly/Downloads/popcorn-compiler/test-install/aarch64/lib/libelf.a(64.xlatetof.o)"(.rodata.xlate64_11); /* size 0x220 */
	. = ALIGN(0x20); /* align for .rodata._elf_fmsize */
	"/home/rlyerly/Downloads/popcorn-compiler/test-install/aarch64/lib/libelf.a(32.fsize.o)"(.rodata._elf_fmsize); /* size 0x220 */
	. = ALIGN(0x1); /* align for .rodata.src_exit_assert__str_Assertion_ */
	"/home/rlyerly/Downloads/popcorn-compiler/test-install/aarch64/lib/libc.a(assert.o)"(.rodata.src_exit_assert__str_Assertion_); /* size 0x23 */
	. = ALIGN(0x10); /* align for .rodata.src_malloc_malloc_c_bin_tab */
	"/home/rlyerly/Downloads/popcorn-compiler/test-install/aarch64/lib/libc.a(malloc.o)"(.rodata.src_malloc_malloc_c_bin_tab); /* size 0x3c */
	. = ALIGN(0x1); /* align for .rodata.src_stdio_fopen__str_rwa_ */
	"/home/rlyerly/Downloads/popcorn-compiler/test-install/aarch64/lib/libc.a(fopen.o)"(.rodata.src_stdio_fopen__str_rwa_); /* size 0x4 */
	. = ALIGN(0x8); /* align for .rodata.stderr */
	"/home/rlyerly/Downloads/popcorn-compiler/test-install/aarch64/lib/libc.a(stderr.o)"(.rodata.stderr); /* size 0x8 */
	. = ALIGN(0x10); /* align for .rodata.src_internal_intscan_c_table */
	"/home/rlyerly/Downloads/popcorn-compiler/test-install/aarch64/lib/libc.a(intscan.o)"(.rodata.src_internal_intscan_c_table); /* size 0x101 */
	. = ALIGN(0x1); /* align for .rodata.src_internal_intscan__str__________ */
	"/home/rlyerly/Downloads/popcorn-compiler/test-install/aarch64/lib/libc.a(intscan.o)"(.rodata.src_internal_intscan__str__________); /* size 0x9 */
	. = ALIGN(0x8); /* align for .rodata.src_signal_block_c_all_mask */
	"/home/rlyerly/Downloads/popcorn-compiler/test-install/aarch64/lib/libc.a(block.o)"(.rodata.src_signal_block_c_all_mask); /* size 0x8 */
	. = ALIGN(0x8); /* align for .rodata.src_signal_block_c_app_mask */
	"/home/rlyerly/Downloads/popcorn-compiler/test-install/aarch64/lib/libc.a(block.o)"(.rodata.src_signal_block_c_app_mask); /* size 0x8 */
	. = ALIGN(0x1); /* align for .rodata.src_stdio___fdopen__str_rwa_ */
	"/home/rlyerly/Downloads/popcorn-compiler/test-install/aarch64/lib/libc.a(__fdopen.o)"(.rodata.src_stdio___fdopen__str_rwa_); /* size 0x4 */
	. = ALIGN(0x8); /* align for .rodata.vfscanf */
	"/home/rlyerly/Downloads/popcorn-compiler/test-install/aarch64/lib/libc.a(vfscanf.o)"(.rodata.vfscanf); /* size 0x338 */
	. = ALIGN(0x1); /* align for .rodata.src_internal_floatscan__str_infinity_ */
	"/home/rlyerly/Downloads/popcorn-compiler/test-install/aarch64/lib/libc.a(floatscan.o)"(.rodata.src_internal_floatscan__str_infinity_); /* size 0x9 */
	. = ALIGN(0x10); /* align for .rodata.src_internal_floatscan_c_decfloat_p10s */
	"/home/rlyerly/Downloads/popcorn-compiler/test-install/aarch64/lib/libc.a(floatscan.o)"(.rodata.src_internal_floatscan_c_decfloat_p10s); /* size 0x20 */
	. = ALIGN(0x4); /* align for .rodata.src_internal_floatscan_switch_table_ */
	"/home/rlyerly/Downloads/popcorn-compiler/test-install/aarch64/lib/libc.a(floatscan.o)"(.rodata.src_internal_floatscan_switch_table_); /* size 0xc */
	. = ALIGN(0x4); /* align for .rodata.src_internal_floatscan_switch_table_2_ */
	"/home/rlyerly/Downloads/popcorn-compiler/test-install/aarch64/lib/libc.a(floatscan.o)"(.rodata.src_internal_floatscan_switch_table_2_); /* size 0xc */
	. = ALIGN(0x10); /* align for .rodata.__fsmu8 */
	"/home/rlyerly/Downloads/popcorn-compiler/test-install/aarch64/lib/libc.a(internal.o)"(.rodata.__fsmu8); /* size 0xcc */
	. = . + 0x4; /* padding before .rodata.src_arch_x86_64_regs_switch_table_ */
	. = . + 0x42; /* padding after .rodata.src_arch_x86_64_regs_switch_table_ */
	*(.rodata);
}
  
  .rodata1        : { *(.rodata1) }
  .eh_frame       : ONLY_IF_RO { KEEP (*(.eh_frame)) }
  
.data	: ALIGN(0x100000)
{
	. = . + 1;
	. = ALIGN(0x1000);
	. = ALIGN(0x8); /* align for .data.src_stdio_stdout_c_f */
	"/home/rlyerly/Downloads/popcorn-compiler/test-install/aarch64/lib/libc.a(stdout.o)"(.data.src_stdio_stdout_c_f); /* size 0xe8 */
	. = ALIGN(0x8); /* align for .data.__stdout_used */
	"/home/rlyerly/Downloads/popcorn-compiler/test-install/aarch64/lib/libc.a(stdout.o)"(.data.__stdout_used); /* size 0x8 */
	. = ALIGN(0x10); /* align for .data.archs */
	"/home/rlyerly/Downloads/popcorn-compiler/test-install/aarch64/lib/libmigrate.a(migrate.o)"(.data.archs); /* size 0x80 */
	. = ALIGN(0x4); /* align for .data.__migrate_flag */
	"/home/rlyerly/Downloads/popcorn-compiler/test-install/aarch64/lib/libmigrate.a(trigger.o)"(.data.__migrate_flag); /* size 0x4 */
	. = ALIGN(0x4); /* align for .data._elf_sanity_checks */
	"/home/rlyerly/Downloads/popcorn-compiler/test-install/aarch64/lib/libelf.a(data.o)"(.data._elf_sanity_checks); /* size 0x4 */
	. = ALIGN(0x8); /* align for .data.src_stdio_stderr_c_f */
	"/home/rlyerly/Downloads/popcorn-compiler/test-install/aarch64/lib/libc.a(stderr.o)"(.data.src_stdio_stderr_c_f); /* size 0xe8 */
	. = ALIGN(0x8); /* align for .data.__stderr_used */
	"/home/rlyerly/Downloads/popcorn-compiler/test-install/aarch64/lib/libc.a(stderr.o)"(.data.__stderr_used); /* size 0x8 */
	. = ALIGN(0x8); /* align for .data.__pthread_tsd_size */
	"/home/rlyerly/Downloads/popcorn-compiler/test-install/aarch64/lib/libc.a(pthread_key_create.o)"(.data.__pthread_tsd_size); /* size 0x8 */
	. = . + 0x8; /* padding after .data.build_x86_64_trigger_o_bc_start */
	. = ALIGN(0x8); /* align for .data.build_aarch64_trigger_o_bc_start */
	"/home/rlyerly/Downloads/popcorn-compiler/test-install/aarch64/lib/libmigrate.a(trigger.o)"(.data.build_aarch64_trigger_o_bc_start); /* size 0x8 */
	*(.data);
}
  
  .data1          : { *(.data1) }
  _edata = .; PROVIDE (edata = .);
/*   . = ALIGN(0x100);   . = .;*/
  __bss_start = .;
  __bss_start__ = .;
  
.bss	: ALIGN(0x100000)
{
	. = . + 1;
	. = ALIGN(0x1000);
	. = ALIGN(0x8); /* align for .bss.__popcorn_stack_base */
	"/home/rlyerly/Downloads/popcorn-compiler/test-install/aarch64/lib/libc.a(__libc_start_main.o)"(.bss.__popcorn_stack_base); /* size 0x8 */
	. = ALIGN(0x8); /* align for .bss.__progname */
	"/home/rlyerly/Downloads/popcorn-compiler/test-install/aarch64/lib/libc.a(libc.o)"(.bss.__progname); /* size 0x8 */
	. = ALIGN(0x8); /* align for .bss.__progname_full */
	"/home/rlyerly/Downloads/popcorn-compiler/test-install/aarch64/lib/libc.a(libc.o)"(.bss.__progname_full); /* size 0x8 */
	. = ALIGN(0x10); /* align for .bss.src_stdio_stdout_c_buf */
	"/home/rlyerly/Downloads/popcorn-compiler/test-install/aarch64/lib/libc.a(stdout.o)"(.bss.src_stdio_stdout_c_buf); /* size 0x408 */
	. = ALIGN(0x8); /* align for .bss.__environ */
	"/home/rlyerly/Downloads/popcorn-compiler/test-install/aarch64/lib/libc.a(__environ.o)"(.bss.__environ); /* size 0x8 */
	. = ALIGN(0x8); /* align for .bss.src_env___init_tls_c_main_tls */
	"/home/rlyerly/Downloads/popcorn-compiler/test-install/aarch64/lib/libc.a(__init_tls.o)"(.bss.src_env___init_tls_c_main_tls); /* size 0x30 */
	. = ALIGN(0x10); /* align for .bss.src_env___init_tls_c_builtin_tls */
	"/home/rlyerly/Downloads/popcorn-compiler/test-install/aarch64/lib/libc.a(__init_tls.o)"(.bss.src_env___init_tls_c_builtin_tls); /* size 0x1a8 */
	. = ALIGN(0x8); /* align for .bss.src_stdio___stdio_exit_c_dummy_file */
	"/home/rlyerly/Downloads/popcorn-compiler/test-install/aarch64/lib/libc.a(__stdio_exit.o)"(.bss.src_stdio___stdio_exit_c_dummy_file); /* size 0x8 */
	. = ALIGN(0x4); /* align for .bss.src_stdio_ofl_c_ofl_lock */
	"/home/rlyerly/Downloads/popcorn-compiler/test-install/aarch64/lib/libc.a(ofl.o)"(.bss.src_stdio_ofl_c_ofl_lock); /* size 0x8 */
	. = ALIGN(0x8); /* align for .bss.src_stdio_ofl_c_ofl_head */
	"/home/rlyerly/Downloads/popcorn-compiler/test-install/aarch64/lib/libc.a(ofl.o)"(.bss.src_stdio_ofl_c_ofl_head); /* size 0x8 */
	. = ALIGN(0x8); /* align for .bss.migrate_callback */
	"/home/rlyerly/Downloads/popcorn-compiler/test-install/aarch64/lib/libmigrate.a(migrate.o)"(.bss.migrate_callback); /* size 0x8 */
	. = ALIGN(0x8); /* align for .bss.migrate_callback_data */
	"/home/rlyerly/Downloads/popcorn-compiler/test-install/aarch64/lib/libmigrate.a(migrate.o)"(.bss.migrate_callback_data); /* size 0x8 */
	. = ALIGN(0x8); /* align for .bss.aarch64_fn */
	"/home/rlyerly/Downloads/popcorn-compiler/test-install/aarch64/lib/libstack-transform.a(userspace.o)"(.bss.aarch64_fn); /* size 0x8 */
	. = ALIGN(0x8); /* align for .bss.powerpc64_fn */
	"/home/rlyerly/Downloads/popcorn-compiler/test-install/aarch64/lib/libstack-transform.a(userspace.o)"(.bss.powerpc64_fn); /* size 0x8 */
	. = ALIGN(0x8); /* align for .bss.x86_64_fn */
	"/home/rlyerly/Downloads/popcorn-compiler/test-install/aarch64/lib/libstack-transform.a(userspace.o)"(.bss.x86_64_fn); /* size 0x8 */
	. = ALIGN(0x8); /* align for .bss.src_userspace_c_aarch64_handle */
	"/home/rlyerly/Downloads/popcorn-compiler/test-install/aarch64/lib/libstack-transform.a(userspace.o)"(.bss.src_userspace_c_aarch64_handle); /* size 0x8 */
	. = ALIGN(0x1); /* align for .bss.src_userspace_c_alloc_aarch64_fn */
	"/home/rlyerly/Downloads/popcorn-compiler/test-install/aarch64/lib/libstack-transform.a(userspace.o)"(.bss.src_userspace_c_alloc_aarch64_fn); /* size 0x1 */
	. = ALIGN(0x8); /* align for .bss.src_userspace_c_powerpc64_handle */
	"/home/rlyerly/Downloads/popcorn-compiler/test-install/aarch64/lib/libstack-transform.a(userspace.o)"(.bss.src_userspace_c_powerpc64_handle); /* size 0x8 */
	. = ALIGN(0x1); /* align for .bss.src_userspace_c_alloc_powerpc64_fn */
	"/home/rlyerly/Downloads/popcorn-compiler/test-install/aarch64/lib/libstack-transform.a(userspace.o)"(.bss.src_userspace_c_alloc_powerpc64_fn); /* size 0x1 */
	. = ALIGN(0x8); /* align for .bss.src_userspace_c_x86_64_handle */
	"/home/rlyerly/Downloads/popcorn-compiler/test-install/aarch64/lib/libstack-transform.a(userspace.o)"(.bss.src_userspace_c_x86_64_handle); /* size 0x8 */
	. = ALIGN(0x1); /* align for .bss.src_userspace_c_alloc_x86_64_fn */
	"/home/rlyerly/Downloads/popcorn-compiler/test-install/aarch64/lib/libstack-transform.a(userspace.o)"(.bss.src_userspace_c_alloc_x86_64_fn); /* size 0x1 */
	. = ALIGN(0x4); /* align for .bss.src_userspace_c_stack_bounds_key */
	"/home/rlyerly/Downloads/popcorn-compiler/test-install/aarch64/lib/libstack-transform.a(userspace.o)"(.bss.src_userspace_c_stack_bounds_key); /* size 0x4 */
	. = ALIGN(0x4); /* align for .bss._elf_fill */
	"/home/rlyerly/Downloads/popcorn-compiler/test-install/aarch64/lib/libelf.a(data.o)"(.bss._elf_fill); /* size 0x4 */
	. = ALIGN(0x4); /* align for .bss._elf_errno */
	"/home/rlyerly/Downloads/popcorn-compiler/test-install/aarch64/lib/libelf.a(data.o)"(.bss._elf_errno); /* size 0x4 */
	. = ALIGN(0x4); /* align for .bss._elf_version */
	"/home/rlyerly/Downloads/popcorn-compiler/test-install/aarch64/lib/libelf.a(data.o)"(.bss._elf_version); /* size 0x4 */
	. = ALIGN(0x8); /* align for .bss.src_malloc_lite_malloc_c___simple_malloc_cur */
	"/home/rlyerly/Downloads/popcorn-compiler/test-install/aarch64/lib/libc.a(lite_malloc.o)"(.bss.src_malloc_lite_malloc_c___simple_malloc_cur); /* size 0x8 */
	. = ALIGN(0x8); /* align for .bss.src_malloc_lite_malloc_c___simple_malloc_end */
	"/home/rlyerly/Downloads/popcorn-compiler/test-install/aarch64/lib/libc.a(lite_malloc.o)"(.bss.src_malloc_lite_malloc_c___simple_malloc_end); /* size 0x8 */
	. = ALIGN(0x4); /* align for .bss.src_malloc_lite_malloc_c___simple_malloc_lock */
	"/home/rlyerly/Downloads/popcorn-compiler/test-install/aarch64/lib/libc.a(lite_malloc.o)"(.bss.src_malloc_lite_malloc_c___simple_malloc_lock); /* size 0x8 */
	. = ALIGN(0x8); /* align for .bss.src_malloc_malloc_c_mal */
	"/home/rlyerly/Downloads/popcorn-compiler/test-install/aarch64/lib/libc.a(malloc.o)"(.bss.src_malloc_malloc_c_mal); /* size 0x610 */
	. = ALIGN(0x4); /* align for .bss.src_malloc_malloc_c_expand_heap_heap_lock */
	"/home/rlyerly/Downloads/popcorn-compiler/test-install/aarch64/lib/libc.a(malloc.o)"(.bss.src_malloc_malloc_c_expand_heap_heap_lock); /* size 0x8 */
	. = ALIGN(0x8); /* align for .bss.src_malloc_malloc_c_expand_heap_end */
	"/home/rlyerly/Downloads/popcorn-compiler/test-install/aarch64/lib/libc.a(malloc.o)"(.bss.src_malloc_malloc_c_expand_heap_end); /* size 0x8 */
	. = ALIGN(0x8); /* align for .bss.src_signal_sigaction_c_handler_set */
	"/home/rlyerly/Downloads/popcorn-compiler/test-install/aarch64/lib/libc.a(sigaction.o)"(.bss.src_signal_sigaction_c_handler_set); /* size 0x8 */
	. = ALIGN(0x1); /* align for .bss.src_signal_sigaction_c_unmask_done */
	"/home/rlyerly/Downloads/popcorn-compiler/test-install/aarch64/lib/libc.a(sigaction.o)"(.bss.src_signal_sigaction_c_unmask_done); /* size 0x1 */
	. = ALIGN(0x8); /* align for .bss.src_stdio_fflush_c_dummy */
	"/home/rlyerly/Downloads/popcorn-compiler/test-install/aarch64/lib/libc.a(fflush.o)"(.bss.src_stdio_fflush_c_dummy); /* size 0x8 */
	. = ALIGN(0x1); /* align for .bss.src_stdio_stderr_c_buf */
	"/home/rlyerly/Downloads/popcorn-compiler/test-install/aarch64/lib/libc.a(stderr.o)"(.bss.src_stdio_stderr_c_buf); /* size 0x8 */
	. = ALIGN(0x10); /* align for .bss.__pthread_tsd_main */
	"/home/rlyerly/Downloads/popcorn-compiler/test-install/aarch64/lib/libc.a(pthread_key_create.o)"(.bss.__pthread_tsd_main); /* size 0x400 */
	. = ALIGN(0x10); /* align for .bss.src_thread_pthread_key_create_c_keys */
	"/home/rlyerly/Downloads/popcorn-compiler/test-install/aarch64/lib/libc.a(pthread_key_create.o)"(.bss.src_thread_pthread_key_create_c_keys); /* size 0x400 */
	. = ALIGN(0x8); /* align for .bss.src_malloc_expand_heap_c___expand_heap_brk */
	"/home/rlyerly/Downloads/popcorn-compiler/test-install/aarch64/lib/libc.a(expand_heap.o)"(.bss.src_malloc_expand_heap_c___expand_heap_brk); /* size 0x8 */
	. = ALIGN(0x4); /* align for .bss.src_malloc_expand_heap_c___expand_heap_mmap_step */
	"/home/rlyerly/Downloads/popcorn-compiler/test-install/aarch64/lib/libc.a(expand_heap.o)"(.bss.src_malloc_expand_heap_c___expand_heap_mmap_step); /* size 0x4 */
	. = ALIGN(0x4); /* align for .bss.src_multibyte_mbrtowc_c_mbrtowc_internal_state */
	"/home/rlyerly/Downloads/popcorn-compiler/test-install/aarch64/lib/libc.a(mbrtowc.o)"(.bss.src_multibyte_mbrtowc_c_mbrtowc_internal_state); /* size 0x4 */
	. = ALIGN(0x8); /* align for .bss.__num_triggers */
	"/home/rlyerly/Downloads/popcorn-compiler/test-install/aarch64/lib/libmigrate.a(trigger.o)"(.bss.__num_triggers); /* size 0x8 */
	*(.bss);
}
  
  _bss_end__ = . ; __bss_end__ = . ;
  . = ALIGN(32 / 8);
  . = ALIGN(32 / 8);
  __end__ = . ;
  _end = .; PROVIDE (end = .);
  . = DATA_SEGMENT_END (.);
  .eh_frame_hdr : { *(.eh_frame_hdr) }
 
 .llvm_stackmaps : ALIGN(0x1000) { *(.llvm_stackmaps) }
  
  .gcc_except_table   : ONLY_IF_RO { *(.gcc_except_table
  .gcc_except_table.*) }
  /* These sections are generated by the Sun/Oracle C++ compiler.  */
  .exception_ranges   : ONLY_IF_RO { *(.exception_ranges
  .exception_ranges*) }
  /* Adjust the address for the data segment.  We want to adjust up to
     the same address within the page on the next page up.  */
  . = ALIGN (CONSTANT (MAXPAGESIZE)) - ((CONSTANT (MAXPAGESIZE) - .) & (CONSTANT (MAXPAGESIZE) - 1)); . = DATA_SEGMENT_ALIGN (CONSTANT (MAXPAGESIZE), CONSTANT (COMMONPAGESIZE));
  /* Exception handling  */
  .eh_frame       : ONLY_IF_RW { KEEP (*(.eh_frame)) }
  .gcc_except_table   : ONLY_IF_RW { *(.gcc_except_table .gcc_except_table.*) }
  .exception_ranges   : ONLY_IF_RW { *(.exception_ranges .exception_ranges*) }
  /* Thread Local Storage sections  */
  
.tdata	: ALIGN(0x100000)
{
	. = . + 1;
	. = ALIGN(0x1000);
}
  
.tbss	: ALIGN(0x100000)
{
	. = . + 1;
	. = ALIGN(0x1000);
}

  .preinit_array     :
  {
    PROVIDE_HIDDEN (__preinit_array_start = .);
    KEEP (*(.preinit_array))
    PROVIDE_HIDDEN (__preinit_array_end = .);
  }
  .init_array     :
  {
    PROVIDE_HIDDEN (__init_array_start = .);
    KEEP (*(SORT(.init_array.*)))
    KEEP (*(.init_array))
    PROVIDE_HIDDEN (__init_array_end = .);
  }
  .fini_array     :
  {
    PROVIDE_HIDDEN (__fini_array_start = .);
    KEEP (*(SORT(.fini_array.*)))
    KEEP (*(.fini_array))
    PROVIDE_HIDDEN (__fini_array_end = .);
  }
  .ctors          :
  {
    /* gcc uses crtbegin.o to find the start of
       the constructors, so we make sure it is
       first.  Because this is a wildcard, it
       doesn't matter if the user does not
       actually link against crtbegin.o; the
       linker won't look for a file to match a
       wildcard.  The wildcard also means that it
       doesn't matter which directory crtbegin.o
       is in.  */
    KEEP (*crtbegin.o(.ctors))
    KEEP (*crtbegin?.o(.ctors))
    /* We don't want to include the .ctor section from
       the crtend.o file until after the sorted ctors.
       The .ctor section from the crtend file contains the
       end of ctors marker and it must be last */
    KEEP (*(EXCLUDE_FILE (*crtend.o *crtend?.o ) .ctors))
    KEEP (*(SORT(.ctors.*)))
    KEEP (*(.ctors))
  }
  .dtors          :
  {
    KEEP (*crtbegin.o(.dtors))
    KEEP (*crtbegin?.o(.dtors))
    KEEP (*(EXCLUDE_FILE (*crtend.o *crtend?.o ) .dtors))
    KEEP (*(SORT(.dtors.*)))
    KEEP (*(.dtors))
  }
  .jcr            : { KEEP (*(.jcr)) }
  .data.rel.ro : 
   {
	*(.data.rel.ro.local* .gnu.linkonce.d.rel.ro.local.*)
	*(.data.rel.ro .data.rel.ro.* .gnu.linkonce.d.rel.ro.*) 
   }
  .dynamic        : { *(.dynamic) }
  .got            : { *(.got) *(.igot) }
  . = DATA_SEGMENT_RELRO_END (24, .);
  .got.plt        : { *(.got.plt)  *(.igot.plt) }
/*  . = ALIGN(32 / 8);
  . = ALIGN(32 / 8);
  __end__ = . ;
  _end = .; PROVIDE (end = .);*/
  . = DATA_SEGMENT_END (.);
  /* Stabs debugging sections.  */
  .stab          0 : { *(.stab) }
  .stabstr       0 : { *(.stabstr) }
  .stab.excl     0 : { *(.stab.excl) }
  .stab.exclstr  0 : { *(.stab.exclstr) }
  .stab.index    0 : { *(.stab.index) }
  .stab.indexstr 0 : { *(.stab.indexstr) }
  .comment       0 : { *(.comment) }
  /* DWARF debug sections.
     Symbols in the DWARF debugging sections are relative to the beginning
     of the section so we begin them at 0.  */
  /* DWARF 1 */
  .debug          0 : { *(.debug) }
  .line           0 : { *(.line) }
  /* GNU DWARF 1 extensions */
  .debug_srcinfo  0 : { *(.debug_srcinfo) }
  .debug_sfnames  0 : { *(.debug_sfnames) }
  /* DWARF 1.1 and DWARF 2 */
  .debug_aranges  0 : { *(.debug_aranges) }
  .debug_pubnames 0 : { *(.debug_pubnames) }
  /* DWARF 2 */
  .debug_info     0 : { *(.debug_info .gnu.linkonce.wi.*) }
  .debug_abbrev   0 : { *(.debug_abbrev) }
  .debug_line     0 : { *(.debug_line) }
  .debug_frame    0 : { *(.debug_frame) }
  .debug_str      0 : { *(.debug_str) }
  .debug_loc      0 : { *(.debug_loc) }
  .debug_macinfo  0 : { *(.debug_macinfo) }
  /* SGI/MIPS DWARF 2 extensions */
  .debug_weaknames 0 : { *(.debug_weaknames) }
  .debug_funcnames 0 : { *(.debug_funcnames) }
  .debug_typenames 0 : { *(.debug_typenames) }
  .debug_varnames  0 : { *(.debug_varnames) }
  /* DWARF 3 */
  .debug_pubtypes 0 : { *(.debug_pubtypes) }
  .debug_ranges   0 : { *(.debug_ranges) }
  /* DWARF Extension.  */
  .debug_macro    0 : { *(.debug_macro) }
  .ARM.attributes 0 : { KEEP (*(.ARM.attributes)) KEEP (*(.gnu.attributes)) }
  .note.gnu.arm.ident 0 : { KEEP (*(.note.gnu.arm.ident)) }
  /DISCARD/ : { *(.note.GNU-stack) *(.gnu_debuglink) *(.gnu.lto_*) *(.note.gnu.build-id) }
}
