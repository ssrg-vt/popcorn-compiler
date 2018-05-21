---------------------------
Setting up the build system
---------------------------

Unfortunately libopenpop uses autoconf/automake to configure & build the
library.  Here are some tips for modifying the build system:

- Change the Makefile

"Makefile.am" provides the template that automake uses to generate
"Makefile.in" (which configure subsequently uses to generate the final
"Makefile").  Make modifications to "Makefile.am" and use "automake" to
generate "Makefile.in".  You may get lots of warnings; that's okay.

This is useful when you need to slightly change how the build proceeds or add
new files to the library.
