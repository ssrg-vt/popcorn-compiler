import Globals
from Globals import er, erStack
import subprocess
import sys
import os
import ReadElfParser
import re
import Symbol

# We put everything common to each arch here
class AbstractArchitecture():
	
###############################################################################
# Getters/Setters
###############################################################################

	def getGccName(self):
		return self._gccPrefix + "-gcc"

	def getArch(self):
		# should be implemented by the concrete class
		raise NotImplementedError

	def getArchString(self):
		raise NotImplementedError

	def getLibGccGoldInclusion(self):
		raise NotImplementedError

	def getObjectFiles(self):
		return self._objectFiles

	def setObjectFiles(self, objectFileList):
		self._objectFiles = objectFileList

#	def createArchSpecificSymbol(self, name, address, size, alignment):
#		raise NotImplementedError

	# Returns the path the the folder containing libgcc.a for the calling 
	# architecture.
	def getLibGccLocation(self):
		gcc_exec_name = self.getGccName()

		try:
			libGccALocation = subprocess.check_output([gcc_exec_name, 
				'-print-libgcc-file-name'], stderr=subprocess.STDOUT)
		except subprocess.CalledProcessError as e:
			sys.stderr.write("ERROR: cannot execute %si" + 
				" -print-libgcc-file-name to find libgcc.a" % gcc_exec_name)
			sys.exit()

		return os.path.dirname(libGccALocation)

###############################################################################
# parseMapFile
##############################################################################

	# Returns a list of Symbols instances extracted from the map file which 
	# path is filePath (should have been generalted by gold.ld -Map <file>
	def parseMapFile(self):
		tmp = 0 #TODO remove me	
		filePath = self.getMapFile()
		res = []

		# The symbols described in the parsed file can mostly have 2 forms: 
		# - 1 line description: "<name> <addr> <size> <alignment> <object file>"
		# - 2 lines description:
		#   "<name>
		#                 <addr> <size> <alignment> <object file>"
		# So here I have 1 regexp for the single line scenario and 2 for the 
		# double lines one. This filters out a lot of uneeded stuff, but we 
		# still need to only keep symbols related to text/data/rodata/bss so 
		# an additional check is performed on the extracted symbosl before 
		# adding it to the result set
		# FIXME: make this more generic, grab all the symbols, and perform
		# filtering later (get only test/bss/etc. ? if we do so there is a check
		# to remove in updateSymbolsList()
		twoLinesRe1 = "^[\s]+(\.[texrodalcbs\.]+[\S]*)$"
		twoLinesRe2 = ("^[\s]+(0x[0-9a-f]+)[\s]+(0x[0-9a-f]+)[\s]+" + 
			"(0x[0-9a-f]+)[\s]+(.*)$")
		oneLineRe = ("^[\s]+(\.[texrodalcbs\.]+[\S]+)[\s]+(0x[0-9a-f]+)[\s]+" + 
			"(0x[0-9a-f]+)[\s]+(0x[0-9a-f]+)[\s]+(.*)$")

		with open(filePath, "r") as mapfile:
			lines = mapfile.readlines()
			for index, line in enumerate(lines):
				s = None
				matchResult = re.match(twoLinesRe1, line)
				if matchResult: # probably a 2-lines symbol description
					nextLine = lines[index+1]
					matchResult2 = re.match(twoLinesRe2, nextLine)
					if matchResult2:
						name = matchResult.group(1)
						address = int(matchResult2.group(1), 0)
						size = int(matchResult2.group(2), 0)
						alignment = int(matchResult2.group(3), 0)
						s = Symbol.Symbol(name, address, size, alignment,
							self.getArch())
					else:
						sys.stderr.write("ERROR! missed a two lines symbol " + 
						"while parsing "	+ "mapfile")
						print "line1: " + line
						print "line2: " + nextLine
						sys.exit(-1)
				else:
					matchResult3 = re.match(oneLineRe, line)
					if matchResult3: # one line symbol description
						name = matchResult3.group(1)
						address = int(matchResult3.group(2), 0)
						size = int(matchResult3.group(3), 0)
						alignment = int(matchResult3.group(4), 0)
						s = Symbol.Symbol(name, address, size, alignment,
							self.getArch())

				if s:
					for section_name in ["text", "data", "rodata", "bss", 
						"tdata", "tbss"]:
						if s.getName().startswith("." + section_name + "."):
							# We are only interested in text/data/rodata/bss
							res.append(s)
		#			print "Symbol found: "
		#			print " " + str(s)
		#			print "Line: "
		#			print " " + line.replace("\n", "")
		#			print "--------------------------------------"
		#		else:
		#			pass
					#print "Unmatched line: " + line.replace("\n", "")
					#print "--------------------------------------"

		return res

###############################################################################
# goldLink
###############################################################################

	# Call gold to link object files with the proper linking options. These
	# options are hardcoded here for now. Some stuff is architecture specific
	# so there are some calls to the concrete class.
	# inputs is a list of object files to link, for ex: ["test.o", "utils.o"]
	def goldLink(self, inputs):
		cmd = []

		gold = Globals.POPCORN_LOCATION + "/bin/ld.gold"
		cmd.append(gold)

		cmd.append("-static")
		cmd.append("--output") 
		cmd.append(self.getExecutable())
		
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
		cmd.append(Globals.POPCORN_LOCATION + "/" + isa_folder + "/lib/crt1.o")
		cmd.append(Globals.POPCORN_LOCATION + "/" + isa_folder + "/lib/libc.a")
		cmd.append(Globals.POPCORN_LOCATION + "/" + isa_folder + 
			"/lib/libmigrate.a")
		cmd.append(Globals.POPCORN_LOCATION + "/" + isa_folder + 
			"/lib/libstack-transform.a")
		cmd.append(Globals.POPCORN_LOCATION + "/" + isa_folder + 
			"/lib/libelf.a")
		cmd.append(Globals.POPCORN_LOCATION + "/" + isa_folder + 
			"/lib/libpthread.a")
		cmd.append(Globals.POPCORN_LOCATION + "/" + isa_folder + "/lib/libc.a")
		cmd.append(Globals.POPCORN_LOCATION + "/" + isa_folder + "/lib/libm.a")

		cmd.append("--start-group")
		search_group = self.getGoldSearchGroup()
		if search_group:
			for lib in search_group.split(" "):
				cmd.append(lib)
		cmd.append("--end-group")

		cmd.append("-Map")
		cmd.append(self.getMapFile())
		cmd.append("--script")
		cmd.append(self.getLinkerScript())

		logfile = Globals.GOLD_LOG_NAME + "_" + self.getArchString() + ".log"
		try:
			gold_output = subprocess.check_output(cmd, 
				stderr=subprocess.STDOUT)
		except subprocess.CalledProcessError as e:
			sys.stderr.write("ERROR: during gold link step:\n" + e.output)
			sys.stderr.write("Command: " + ' '.join(cmd) + "\n")
			sys.stderr.write("Output:\n" + e.output)
			with open(logfile, "w+") as f:
				f.write(e.output)
				sys.exit()

		with open(logfile, "w+") as f:
			f.write(gold_output)

		return

###############################################################################
# getSection
###############################################################################

	# Return the section associated with a symbol as a string (ex: ".text")
	# symbol is a Symbol instance, sectionInfo is a list of Section object, the
	# list that is returned by ReadElfParser.getSectionInfo()
	def getSection(self, symbol, sectionInfo):
		res = "." + symbol.getName().split(".")[1]
		sectionsNames = [section.getName() for section in sectionInfo]
		if res in sectionsNames:
			# Sanity check on the address, is it falling into the section?
			section = next((s for s in sectionInfo if s.getName() == res))
			if not section.checkAddressInSection(symbol.getAddress(
				self.getArch())):
				er("ERROR: symbol address not falling into the expected " +
					"section range\n")
				er("Symbol: " + str(symbol) + "\nSection: " + str(section) +
					"\n")
				erStack("Arch: " + self.getArchString() + "\n")
				sys.exit(-1)
		else:
			er("ERROR: symbol not in extracted section list:\n")
			erStack(str(symbol))
			sys.exit(-1)

		return res

###############################################################################
# updateSymbolsList
###############################################################################

	# symbolsList is a dictionary of lists, one per section, for example:
	# symbolsList = { ".text" : [], ".rodata" : [], ".bss" : [], ".data" : [], 
	# ".tdata" : [], ".tbss" : [] }
	# This function TODO update description and maybe change the name
	# accordinglyn
	# way to call it:
	# Arch1.updateSymbolsList(list)
	# Arch2.updateSymbolsList(list)
	# Arch3.updateSymbolsList(list)
	# etc.
	def updateSymbolsList(self, symbolsList):
		# Grab info about sections from the executable
		sectionsInfo = ReadElfParser.getSectionInfo(self.getExecutable())
		
		# Grab symbols from the map file
		symbolsToAdd = self.parseMapFile()

		for symbol in symbolsToAdd:
			# TODO: here in Chris's script they merge multiple symbols like 
			# that:
			# [section.subsection.symbol1, section.subsection.symbol2]
			#   --> section.subsection.*
			# This is based on a super arbitrary rule:
			# if (name.IndexOf(".", 8) > 7)
			# should I do the same .....s
			# TODO:
			
			# First find the section
			sectionName = self.getSection(symbol, sectionsInfo)

			# TODO remove this check if the symbol parser does not return only
			# the symbols from the considered sections
			if sectionName not in symbolsList.keys():
				sys.stderr.write("ERROR: found a symbol from a " +
					"non-considered section\n")
				sys.exit(-1)

			# is there already a symbol with that name in symbolLists?
			currentSymbolsNames = [s.getName() for s in 
				symbolsList[sectionName]]
			if symbol.getName() not in currentSymbolsNames:
				# TODO here Chris is checking if there is no other symbol
				# with the same address and seting a flag in that symbol
				# if it is the case, it this really needed?
				symbolsList[sectionName].append(symbol)
			else:
				pass
				#TODO
				#print "Duplicate symbol, want to add:"
				#print str(symbol)
				#print "but this is already in the list:"
				#already_there = next((s for s in symbolsList[sectionName] if 
				#	s.getName() == symbol.getName()))
				#print already_there
				#print "------------------------------------------------------"

