import Globals
from Globals import er, erStack, warn
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

	
	def getLinkerScript(self):
		return self._linkerScript

	def setLinkerScript(self, ls):
		self._linkerScript = ls

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
			er("cannot execute %s -print-libgcc-file-name to find libgcc.a" 
				% gcc_exec_name)
			sys.exit()

		return os.path.dirname(libGccALocation)

###############################################################################
# parseMapFile
##############################################################################

	# Returns a list of Symbols instances extracted from the map file which 
	# path is filePath (should have been generalted by gold.ld -Map <file>
	def parseMapFile(self):
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
						objectFile = matchResult2.group(4)
						s = Symbol.Symbol(name, address, size, alignment, 
						objectFile, self.getArch())
					else:
						er("missed a two lines symbol while parsing	mapfile:\n")
						er("line1: " + line + "\n")
						er("line2: " + nextLine + "\n")
						sys.exit(-1)
				else:
					matchResult3 = re.match(oneLineRe, line)
					if matchResult3: # one line symbol description
						name = matchResult3.group(1)
						address = int(matchResult3.group(2), 0)
						size = int(matchResult3.group(3), 0)
						alignment = int(matchResult3.group(4), 0)
						objectFile = matchResult3.group(5)
						s = Symbol.Symbol(name, address, size, alignment,
							objectFile, self.getArch())

				if s:
					res.append(s)

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
			er("during gold link step:\n" + e.output)
			er("Command: " + ' '.join(cmd) + "\n")
			er("Output:\n" + e.output)
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
		arch = self.getArch()
		addr = symbol.getAddress(arch)
		res = None

		for section in sectionInfo:
			sectionAddr = section.getAddress()
			sectionSize = section.getSize()
			sectionName = section.getName()
			if (addr >= sectionAddr) and (addr < (sectionAddr + sectionSize)):
				res = sectionName
				if not symbol.getName().startswith(sectionName):
				# Sanity check if the names fit. is it possible to have a symbol
				# name _not_ starting with the containing section name?
					warn("symbol does not have the same name as the " +
							"containing section\n")
					warn(" Symbol: " + symbol.getName() + " @" + 
						str(hex(symbol.getAddress(arch))) + ", size: " +
						str(hex(symbol.getSize(arch))) + "\n")
					warn(" Section: " + sectionName + " @" + 
						str(hex(sectionAddr)) + ", size: " + 
						str(hex(sectionSize)))
						
				break

			# Super special case, I have seen this in some map files, don't
			# really know what it means ...
			if (addr == (sectionAddr + sectionSize)):
				if symbol.getName().startswith(sectionName): #check name
					res = sectionName
					#warn("symbol at section_end + 1:\n " + str(symbol) + 
					#	"\n Section: " + str(section) + "\n")
					break

		if not res:
			#print (self.getArchString() + ": " + symbol.getName() + 
			#	" not in considered sections")
			# The symbol is not in the considered sections, pass
			pass

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
		arch = self.getArch()

		# Grab info about sections from the executable
		consideredSections = symbolsList.keys()
		sectionsInfo = ReadElfParser.getSectionInfo(self.getExecutable(),
			filterSections=consideredSections)

		# Grab symbols from the map file
		symbolsToAdd = self.parseMapFile()

		for symbol in symbolsToAdd:

			# First find the section
			sectionName = self.getSection(symbol, sectionsInfo)
			if not sectionName:  #Symbol not in one of the considered sections
				continue
			
			# is the symbol blacklisted?
			if sectionName in Globals.SYMBOLS_BLACKLIST.keys():
				if symbol.getName() in Globals.SYMBOLS_BLACKLIST[sectionName]:
					continue

			updated = False
			for existingSymbol in symbolsList[sectionName]:
				if symbol.compare(existingSymbol):
					# Found similar symbol in another arch ...
					if existingSymbol.getReference(arch): #... or not
						er("Already referenced updated symbol: " + 
						existingSymbol.getName() + "|" + 
						existingSymbol.getObjectFile(arch) +
						"|" + str(hex(existingSymbol.getAlignment(arch))) + 
						"|" +str(hex(symbol.getAlignment(arch))) +
						" (" +
						self.getArchString() + ")\n")
						sys.exit(-1)

					existingSymbol.setAddress(symbol.getAddress(arch), arch)
					existingSymbol.setSize(symbol.getSize(arch), arch)
					existingSymbol.setAlignment(symbol.getAlignment(arch),
						arch)
					existingSymbol.setReference(arch)
					existingSymbol.setObjectFile(symbol.getObjectFile(arch), arch)
					updated = True
					break

			if not updated:
				symbolsList[sectionName].append(symbol)
