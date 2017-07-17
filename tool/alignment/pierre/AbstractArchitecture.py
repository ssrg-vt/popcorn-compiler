from Globals import *
import subprocess
import sys

# We put everything common to each arch here
class AbstractArchitecture():
	
	def getGccName(self):
		return self._gcc_prefix + "-gcc"

	# Returns the path the the folder containing libgcc.a for the calling 
	# architecture. This assume a x86 host system with libgcc.a being provided
	# by cross-compilers for arm64 and powerpc
	def getLibGccLocation(self):
		gcc_exec_name = self.getGccName()
		try:
			gcc_version = subprocess.check_output([gcc_exec_name, 
				'-dumpversion'], stderr=subprocess.STDOUT)
		except Exception:
			sys.stderr.write("ERROR: cannot execute %s -dumpversion to find" +
				" libgcc.a")
			sys.exit()

		gcc_version = gcc_version.replace("\n", "")
		libgcc_path = ""
		if self.getCrossCompile():
			libgcc_path = ("/usr/lib/gcc-cross/" + 
				gcc_exec_name.replace("-gcc", "") + "/" +	gcc_version)
		else:
			libgcc_path = ("/usr/lib/gcc/" + gcc_exec_name.replace("-gcc", "") 
				+ "/" + gcc_version)

		return libgcc_path

	# Call gold to link object files with the proper linking options. These
	# options are hardcoded here for now. Some stuff is architecture specific
	# so there are some calls to the concrete class.
	# inputs is a list of object files to link, for ex: ["test.o", "utils.o"]
	def goldLink(self, inputs):
		cmd = []

		gold = POPCORN_LOCATION + "/bin/ld.gold"
		cmd.append(gold)

		cmd.append("-static")
		cmd.append("--output") 
		cmd.append(self.getGoldOutput())
		
		for object_file in inputs:
			cmd.append(object_file)

		cmd.append(self.getLibGccGoldInclusion())
		
		cmd.append("-z")
		cmd.append("relro")
		cmd.append("--hash-style=gnu")
		cmd.append("--build-id")
		cmd.append("-m")
		cmd.append(self.getGoldEmulation())		

		isa_folder = self.getPopcornIsaFolder()
		cmd.append(POPCORN_LOCATION + "/" + isa_folder + "/lib/crt1.o")
		cmd.append(POPCORN_LOCATION + "/" + isa_folder + "/lib/libc.a")
		cmd.append(POPCORN_LOCATION + "/" + isa_folder + "/lib/libmigrate.a")
		cmd.append(POPCORN_LOCATION + "/" + isa_folder + 
			"/lib/libstack-transform.a")
		cmd.append(POPCORN_LOCATION + "/" + isa_folder + "/lib/libelf.a")
		cmd.append(POPCORN_LOCATION + "/" + isa_folder + "/lib/libpthread.a")
		cmd.append(POPCORN_LOCATION + "/" + isa_folder + "/lib/libc.a")
		cmd.append(POPCORN_LOCATION + "/" + isa_folder + "/lib/libm.a")

		cmd.append("--start-group")
		search_group = self.getGoldSearchGroup()
		if search_group:
			for lib in search_group.split(" "):
				cmd.append(lib)
		cmd.append("--end-group")

		cmd.append("-Map")
		cmd.append(self.getGoldMap())
		cmd.append("--script")
		cmd.append(self.getLinkerScript())

		try:
			gold_output = subprocess.check_output(cmd, 
				stderr=subprocess.STDOUT)
		except Exception as e:
			sys.stderr.write("ERROR: during gold link step:\n" + e.output)
			sys.stderr.write("Command: " + ' '.join(cmd) + "\n")
			with open(self.getLinkerLog(), "w+") as f:
				f.write(e.output)
				sys.exit()

		with open(self.getLinkerLog(), "w+") as f:
			f.write(gold_output)

		return

	#	def getSectionInfo(binaryPath):

