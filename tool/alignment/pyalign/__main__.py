"""
Program entry point
"""
import argparse, os, sys
import X86, Arm, Power, Linker, Globals
from Arch import Arch
from Globals import er

# Instantiate one object representing each architecture and put them
# in a dictionary indexed by the Arch "enum" (see Arch.py)
x86_obj = X86.X86()
arm_obj = Arm.Arm()
power_obj = Power.Power()

archs = {	Arch.X86 : x86_obj,
			Arch.ARM : arm_obj,
			Arch.POWER : power_obj}

#TODO add power later here
considered_archs = [archs[Arch.X86], archs[Arch.ARM]]
considered_sections = [".text", ".data", ".tdata", ".tbss", ".bss"]
#, ".rodata"]

# TODO add power later here
def buildArgParser():
	""" Construct the command line argument parser object """

	res = argparse.ArgumentParser(description="Align symbols in binaries from" +
		" multiple ISAs")

        res.add_argument("--compiler-inst", help="Path to the compiler installation",
                required=True)
	res.add_argument("--x86-bin", help="Path to the input x86 executable",
		required=True)
	res.add_argument("--arm-bin", help="Path to the input ARM executable",
		required=True)
	res.add_argument("--x86-map", help="Path to the x86 memory map file " +
		"corresponding to the x86 binary (generated through ld -MAP)",
		required=True)
	res.add_argument("--arm-map", help="Path to the ARM memory map file " +
		"corresponding to the ARM binary (generated through ld -MAP)",
		required=True)
	res.add_argument("--work-dir", help="Temporary work directory",
		default="align")
	res.add_argument("--clean-work-dir", type=bool,
		help="Delete work directory when done",	default=True)
	res.add_argument("--output-x86-ls", help="Path to the output x86 linker " +
	"script", default="linker_script_x86.x")
	res.add_argument("--output-arm-ls", help="Path to the output ARM linker " +
	"script", default="linker_script_arm.x")

	return res

def parseAndCheckArgs(parser):
	""" Parse command line arguments and perform some sanity checks """
	args = parser.parse_args()
	# TODO checks here (ex wit power we can have 2 or 3 architectures to take
	# care of

	return args

def setInputOutputs(args):
	""" Fill internal data structures with info about input and ouput files """

	archs[Arch.X86].setExecutable(os.path.abspath(args.x86_bin))
	archs[Arch.ARM].setExecutable(os.path.abspath(args.arm_bin))
	archs[Arch.X86].setMapFile(os.path.abspath(args.x86_map))
	archs[Arch.ARM].setMapFile(os.path.abspath(args.arm_map))
	archs[Arch.X86].setLinkerScript(os.path.abspath(args.output_x86_ls))
	archs[Arch.ARM].setLinkerScript(os.path.abspath(args.output_arm_ls))

def orderSymbolList(sl):
	"""In each section, the symbols will be ordered by number of architectures
	referencing the symbols in questions
	sl is a list of Symbol instances (should correspond to one section)
	return as a result an ordered symbol list
	"""
	res = []

	# Order the list of symbols based on the number of referencing architectures
	# (for each symbol). 
	for order in list(reversed(range(1, len(considered_archs)+1))):
		for symbol in sl:

			referencingArchs = len(considered_archs)
			for arch_instance in considered_archs:
				arch = arch_instance.getArch()
				if symbol.getAddress(arch) == -1:  # not referenced by this arch
					referencingArchs -= 1

			if referencingArchs == order:
				res.append(symbol)

	return res

def align(sl):
	""" This is the core of the alignment logic
	sl is a list of symbols (same as for orderSymbolsList that should correspond
	to one section
	here is what we do:

	   Arch A           Arch B            Arch C          Arch D
	                                                    (symbol not
	                                                         present)
	+------------+  +-------------+  +--------------+  +-------------+
	| Pad before |  | Pad before  |  | Pad before   |  |  Pad before |
	+------------+  +-------------+  +--------------+  +-------------+
	+------------+  +-------------+  +--------------+  +-------------+
	| Symbol     |  |             |  |     Symbol   |  |             |
	|            |  |             |  |              |  |             |
	|            |  |   Symbol    |  +--------------+  |             |
	+------------+  |             |  +--------------+  | pad after   |
	+------------+  |             |  |              |  |             |
	|Pad after   |  |             |  |  pad after   |  |             |
	|            |  |             |  |              |  |             |
	|            |  |             |  |              |  |             |
	+------------+  +-------------+  +--------------+  +-------------+

	"pad before" automatically added by the linker to satisfy the alignment
	constraint we put on the symobl (we choose the largest alignment constaint
	of 	all referencing architectures. We need to add it by hand on each
	architecture that is not referencing the symbol, though.
	"pad after" is added by us so that the next symbol to be processed starts at
	the same address on each architecture
	"""
	address = 0

	for symbol in sl:
		padBefore = 0  # padding we'll need to add before that symbol
		# set the alignment for all referencing arch to the largest one
		alignment = symbol.setLargestAlignment()
		while ((address + padBefore) % alignment) != 0:
			padBefore += 1

		# Add padding "before" for architectures not referencing the symbol
		for arch in symbol.getArchitecturesNotReferencing():
			symbol.incrPaddingBefore(padBefore, arch)

		# Now get the largest symbol size among archs to compute paddign to put
		# after the symbol on
		largestSize = symbol.getLargetSizeVal()

		# Add some padding to referencing archs for which the size is smaller
		# than the largest one
		for arch in symbol.getArchitecturesReferencing():
			padAfter = largestSize - symbol.getSize(arch)
			symbol.incrPaddingAfter(padAfter, arch)

		# For the architecture not referencing the symbol, we should add padding
		# equals to the largest size
		for arch in symbol.getArchitecturesNotReferencing():
			symbol.incrPaddingAfter(largestSize, arch)

		# increment our "iterator" so we can compute the padding "before" to
		# add on the next symbol to process
		address += padBefore + largestSize

	return sl

def printBySection(sl):
	for symbol in sl:
		print(symbol.getName())

if __name__ == "__main__":
	# Argument parsing stuff
	parser = buildArgParser()
	args = parseAndCheckArgs(parser)
        Globals.POPCORN_LOCATION = args.compiler_inst

	# Grab input/output files from the command line arguments
	setInputOutputs(args)

	# List of per-section symbols that we are going to fill and manipulate
	work = {}
	for section in considered_sections:
		work[section] = []

	# Add symbols to the list
	for arch in considered_archs:
		arch.updateSymbolsList(work)
	'''
	for arch in considered_archs:
		arch.printSymbols(work)
	
	for section in considered_sections:
		printBySection(work[section])
	'''
	# Order symbols in each section based on the number of arch referencing
	# each symbol
	for section in considered_sections:
		work[section] = orderSymbolList(work[section])

	# perform the alignment
	for section in considered_sections:
		work[section] = align(work[section])

	# write linker scripts
	for arch in considered_archs:
		Linker.Linker.produceLinkerScript(work, arch)
