import sys, traceback

## Global variables and utility functions

# Popcorn toolchain install dir:
POPCORN_LOCATION="/usr/local/popcorn"

###############################################################################
# Subdirs of the working directory we put the object files into
X86_OBJS_SUBDIR="objs_x86"
ARM_OBJS_SUBDIR="objs_arm"
POWER_OBJS_SUBDIR="objs_power"

# File name for gold output log (will be suffixed with the arch name)
GOLD_LOG_NAME="gold_log"

def er(string):
	sys.stderr.write(string)

def erStack(string):
	er(string)
	for line in traceback.format_stack():
		print(line.strip())
