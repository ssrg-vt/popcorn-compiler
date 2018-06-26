"""
Global variables and utility functions
"""

import sys, traceback

# Popcorn toolchain install dir:
POPCORN_LOCATION="/usr/local/popcorn/"

###############################################################################
# Error printing stuff

class ercolors:
    WARNING = '\033[93m'
    FAIL = '\033[91m'
    ENDC = '\033[0m'

def warn(string):
	sys.stderr.write(ercolors.WARNING + "WARNING: " + string + ercolors.ENDC)

def er(string):
	sys.stderr.write(ercolors.FAIL + "ERROR: " + string + ercolors.ENDC)

def erStack(string):
	er(string)
	for line in traceback.format_stack():
		print(line.strip())

###############################################################################
# Symbol blacklist
# In the input map file, we can multiple symbols having the same name. In
# particular, "symbols" (are these really symbols) having the same name as
# sections: .text, .data, etc. My guess is: these are just "residues" of the
# -ffunction-sections and -fdata-sections operations: you end up with all
# functions / data in their own section but there are still empty .text, .data,
# .bss sections.
# These are not reported as symbols by nm, so I'm going to ignore them for now.
# It is actually possible to align 99% of them if we want, but there are a few
# corner cases for which I don't think there is a simple solution. These cases
# are related to object files contained in libstack-transform.a (properties.o
# and regs.o). They contain multiple occurences of these .text, .bss, and .data
# "symbols". It is not possible to refer to each one of these uniquely in the
# linker script I'm building, so it's not possible to align them properly under
# certain circumstences
SYMBOLS_BLACKLIST={	".text" : [".text"],
					".data" : [".data"],
					".bss" : [".bss"],
					".rodata" :[".rodata"] }
