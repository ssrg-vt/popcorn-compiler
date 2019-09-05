"""
Part of the code responsible for the generation of the linker scripts
"""
from Arch import Arch
import Globals
import re

class Linker:

	_sectionmarker = {	".text" 			: "__TEXT__",
						".data" 			: "__DATA__",
						".bss"  			: "__BSS__",
						".rodata" 			: "__RODATA__",
						".tdata" 			: "__TDATA__",
						".tbss" 			: "__TBSS__" }

	@classmethod
	def getSectionMarker(cls, sectionName):
		return cls._sectionmarker[sectionName]

	# symbolsList should be a dictionary of per-section Symbol lists, ex:
	# { ".text" : [S1, S2, etc.], ".data" : [S1, S2, etc.], etc. }
	# arch is an Arch instance
	@classmethod
	def produceLinkerScript(cls, symbolsList, arch):
		""" linker script generation method
		"""
		template = arch.getLsTemplate()
		archEnum = arch.getArch()

		with open(template, "r") as f:
			lines = f.readlines()

		output_buffer = []
		for line in lines:
			foundMarker = False
			for section in symbolsList.keys():
				if line.startswith(cls.getSectionMarker(section)):
					foundMarker = True
					# Section "opening" part
					# FIXME: We rely on this super coarse-grain alignment
					# to have the sections start at the same offset on each
					# architecture -> there is probably a more intelligent way

                                        # TEXT and RODATA require special alignment when linking DSOs in
                                        # one ISA and static libraries in the other
                                        if section == ".text":
                                                output_buffer.append(".text 0x700000: ALIGN(0x100000)\n")
                                        elif section == ".rodata":
                                                output_buffer.append(".rodata 0x900000: ALIGN(0x100000)\n")
                                        else:
					        output_buffer.append(section + "\t: ALIGN(0x100000)\n")
					output_buffer.append("{\n")

                                        # Correct TBSS
                                        if section == ".tbss" and not symbolsList[section]:
                                                output_buffer.append("\t*(.tbss .tbss.* .gnu.linkonce.tb.*) *(.tcommon)\n")
                                                output_buffer.append("}\n")
                                                break;

                                        # Correct TDATA
                                        if section == ".tdata":
                                                output_buffer.append("\tPROVIDE_HIDDEN (__tdata_start = .);\n")
                                                output_buffer.append("\t*(.tdata .tdata.* .gnu.linkonce.td.*);\n")
                                                if not symbolsList[section]:
                                                        output_buffer.append("}\n")
                                                        break;

                                        # Correct BSS
                                        if section == ".bss" and archEnum == Arch.X86:
                                                output_buffer.append("\t*(.dynbss)\n")
                                                #output_buffer.append("\t*(.bss .bss.* .gnu.linkonce.b.*)\n")

					# FIXME: sometimes the linker fills the start of some
					# sections (at least .data) with something named **common,
					# only for a subset of the considered archs. I'm not sure
					# how to force this **common to be somewhere else like in
					# the end of the section with the other symbols that are
					# referenced only by one architecture. With this trick
					# I can still align what is below the **common, but if
					# **common size is larger than 0x1000 this number will
					# have to be increased...
					output_buffer.append("\t. = . + 1;\n")
					output_buffer.append("\t. = ALIGN(0x1000);\n")

					# iterate over symbols to add:
					for symbol in symbolsList[section]:
						# First add padding before if needed
						if not symbol.getReference(archEnum):
							padding_before = symbol.getPaddingBefore(archEnum)
							if padding_before:
								output_buffer.append("\t. = . + " +
									hex(padding_before) +
									"; /* padding before "+
									symbol.getName() + " */\n")

						# check if the symbol is actually enabled for this arch
						if symbol.getSize(arch.getArch()) >= 0:
							output_buffer.append("\t. = ALIGN(" +
								str(hex(symbol.getAlignment(archEnum))) +
								"); /* align for " + symbol.getName() + " */\n")

							output_buffer.append("\t\"" +
							getObjectFileFromSymbol(symbol, archEnum) + "\"(" +
							symbol.getName() + "); /* size " +
							hex(symbol.getSize(archEnum)) +	" */\n")

						# Then add padding after if needed
						padding_after = symbol.getPaddingAfter(archEnum)
						if padding_after:
							output_buffer.append("\t. = . + " +
								hex(padding_after) + "; /* padding after " +
								symbol.getName() + " */\n")

					# Put the set of blacklisted symbols in the end
					if section in Globals.SYMBOLS_BLACKLIST.keys():
						for symbolName in Globals.SYMBOLS_BLACKLIST[section]:
							output_buffer.append("\t*(" + symbolName + ");\n")

					# Section "closing" part
					output_buffer.append("}\n")

			if not foundMarker:
				output_buffer.append(line)

			with open(arch.getLinkerScript(), "w") as f:
				for line in output_buffer:
					f.write(line)

def getObjectFileFromSymbol(symbol, archEnum):
	# When specifying individual object files inside library
	# archives, LD.BFD expects the form 'Library:ObjectFile',
	# rather than Library(ObjectFile).
	file = symbol.getObjectFile(archEnum)
	
	reLib = "^(.+\.a)\((.+\.o)\)" # To check if it comes from an archive
	lib = re.match(reLib, file)

	if (not lib):
	        return file
	
	#print (lib.group(1) + ":" + lib.group(2))
	return lib.group(1) + ":" + lib.group(2)
