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

# Math stuff

# Greatet common divider
def gcd(x, y):
	while y:
		tmp = y
		y = x % y
		x = tmp
	return x

# least common multiple
def lcm(x, y):
	return x * y // gcd(x, y)
