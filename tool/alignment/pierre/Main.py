import X86, Arm, Power
import argparse
import os, sys, shutil
from Arch import Arch

# Instantiate one object representing each architecture and put them
# in a dictionary indexed by the Arch "enum" (see Arch.py)
x86_obj = X86.X86()
arm_obj = Arm.Arm()
power_obj = Power.Power()

Archs = { 	Arch.X86 : x86_obj,
			Arch.ARM : arm_obj,
			Arch.POWER : power_obj,
		}

considered_archs = [Archs[Arch.X86]] #TODO add arm and power later
considered_sections = [".text", ".data", ".bss", ".rodata", ".tdata", 
		".tbss"]

###############################################################################
# buildArgparser
###############################################################################
def buildArgParser():
	""" Construct the command line argument parser object """

	res = argparse.ArgumentParser(description="Align symbols in binaries from" +
		" multiple ISAs")
	
	res.add_argument("--x86-bin", help="Path to the input x86 executable",
		required=True)
	res.add_argument("--arm-bin", help="Path to the input ARM executable",
		required=True)
	res.add_argument("--x86-map", help="Path to the x86 memory map file " +
		"corresponding to the x86 binary (generated through ld -MAP)",
		required=True)
	res.add_argument("--arm-map", help="Path to the ARM memory map file " +
		"corresponding to the x86 binary (generated through ld -MAP)",
		required=True)
	res.add_argument("--work-dir", help="Temporary work directory",
		default="/tmp/align")
	res.add_argument("--clean-work-dir", type=bool, 
		help="Delete work directory when done",	default=True)

	return res

###############################################################################
# prepareWorkDir
###############################################################################
def prepareWorkDir(workDir, x86Bin, armBin, x86Map, armMap):
	""" Clean the working directory and copy input files in it """
	workdir = args.work_dir
	if os.path.exists(workdir):
		if not os.path.isdir(workdir):
			sys.stderr.write("ERROR: " + workdir + " exists and is not a " +
				"directory, remove it first")
			sys.exit(-1)
		shutil.rmtree(workdir)

	os.makedirs(workdir)
	shutil.copyfile(args.x86_bin, workdir + "/" + 
		Archs[Arch.X86].getExecutable())
	shutil.copyfile(args.arm_bin, workdir + "/" + 
		Archs[Arch.ARM].getExecutable())
	shutil.copyfile(args.x86_map, workdir + "/" + 
		Archs[Arch.X86].getMapFile())
	shutil.copyfile(args.arm_map, workdir + "/" + 
		Archs[Arch.ARM].getMapFile())

	return

###############################################################################
# main
###############################################################################
if __name__ == "__main__":
	parser = buildArgParser()
	args = parser.parse_args()
	
	prepareWorkDir(args.work_dir, args.x86_bin, args.arm_bin, args.x86_map,
		args.arm_map)
	# Switch to the workdir as CWD
	os.chdir(args.work_dir)

	# List of per-section symbols that we are going to fill and manipulate
	work = {}
	for section in considered_sections:
		work[section] = []

	for arch in considered_archs:
		arch.updateSymbolsList(work)
	
