Modified binutils gold linker in arm.cc and x86_64.cc to bypass
an issue with relocation parsing in our new -Bsymbolic -shared test

 default:
      {
        // This prevents us from issuing more than one error per reloc
        // section.  But we can still wind up issuing more than one
        // error per object file.
        if (this->issued_non_pic_error_)
          return;
#if 0
        const Arm_reloc_property* reloc_property =
          arm_reloc_property_table->get_reloc_property(r_type);
        gold_assert(reloc_property != NULL);
        object->error(_("requires unsupported dynamic reloc %s; "
                      "recompile with -fPIC"),
                      reloc_property->name().c_str());
        this->issued_non_pic_error_ = true;
        return;
#endif
        break;

May 5th - Today I built multia-isa-bin with a Makefile that is using
a slightly modified version of the compiler that allows for using -shared -Bsymbolic
which results in all kinds of map parsing errors we ignore with another code patch
in the gold linker. The result is a binary that is totally wrong and doesn't work.

I've gone back to the drawing board of just building all of the libraries as PIC,
building the binary as a standard static binary with align of 0x1000 (pyelf modification)
and a linker script starting at text 0x0. And rcrt1.o--

-- 5/6/2021

When we use the GNU ld linker instead of ld.gold, we can pass the LDFLAGS
-shared -Bsymbolic -static, and this creates a PIE executable that is static.
It does however still create relative relocations that it expects to be
fixed up by a dynamic linker... we need to build libmusl so that it doesn't
require any fixups.

Also the libmigrate code is on its way to being fully patched so that it uses
position independent solutions within the inline ASM. One patch has been added
and another one is required.

