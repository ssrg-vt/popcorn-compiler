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
	#				for section_name in ["text", "data", "rodata", "bss", 
	#					"tdata", "tbss"]:
	#					if s.getName().startswith("." + section_name + "."):
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
					warn("WARNING: symbol does not have the same name as the " +
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
			#	res = sectionName
				warn("WARNING: symbol at section_end + 1:\n" + str(symbol) + 
					"\n")
				break

		if not res:
			#print (self.getArchString() + ": " + symbol.getName() + 
			#	" not in considered sections")
			# The symbol is not in the considered sections, pass
			pass

		return res
###############################################################################
# recomputeSizeOnNewAlignment
###############################################################################

	def recomputeSizeOnNewAlignment(self, symbol, newAlignment):
		arch = self.getArch()
		size = 0

		# Align each existing internal symbol
		for internalSymbol in symbol.getInternalSymbols(arch):
			size += internalSymbol
			while (size % newAlignment) != 0:
				size += 1

###############################################################################
# updateSymbolInSymbolsList
###############################################################################

	def updateSymbolInSymbolsList(self, symbolToUpdate, sectionName, 
		symbolsList):
		
		arch = self.getArch()

		# First search for the symbol in the list
		for symbol in (symbolsList[sectionName]):
			if symbol.getName() == symbolToUpdate.getName():
				# is it the first time we touch taht symbol for this arch?
				# Just add the symbol to the list
				if symbol.getAddress(arch) == -1: #TODO replace with reference
					symbol.setAddress(symbolToUpdate.getAddress(arch), arch)
					symbol.setSize(symbolToUpdate.getSize(arch), arch)
					symbol.setAlignment(symbolToUpdate.getAlignment(arch), arch)
					symbol.setReference(arch)
					symbol.setObjectFile(symbolToUpdate.getObjectFile(arch), 
						arch)
					symbol.addInternalSymbol(symbolToUpdate.getSize(arch), arch)
				else:
				# We found a duplicate symbol for this architecture. We are
				# going to pack all the symbols with the same name one after
				# the other in the address space, and that set is represented
				# by a single instance of Symbol in our representation.
				# we need to compute the size of this set, and the difficulty is
				# that each symbol inside the set might have different alignment
				# constraint. So when we find a dulpicate symbol 'sd' we must 
				# update the Symbol 'se' representing the set as follows:
				# 1. If sd alignment constraint is greater than se, set se 
				#    constraint equal to the one of sd. 
				# 2. If se alignment constraint is greater than the constraint 
				#    of sd, then set sd constraint to the one of se.
				# 3. TODO multiple shit
				# That way, we assure that each symbol inside the set will have 
				# the same alignment constraint so we can make assumption about 
				# the padding that the linker will add before the symbol we are 
				# currently inserting: the size of the set after insertion will 
				# then be: old_set_size + some_padding + inserted_symbol_size.
				# some_padding can be easily computed as we ensured than the 
				# first symbol of the set and the inserted one have the same
				# alignment constraints

					# Compute the new alignment
					existingAl = symbol.getAlignment(arch)
					insertedAl = symbolToUpdate.getAlignment(arch)
					newAl = max(existingAl, insertedAl)
					if ((existingAl % insertedAl != 0) or
						(insertedAl % existingAl != 0)):
						newAl = Globals.lcm(existingAl , insertedAl)
					#	print "Fancy alignment: " + str(hex(newAl))
					#	print " existing was:" + str(hex(existingAl))
					#	print " inserted is :" + str(hex(insertedAl))

					if newAl != existingAl:
					# we need to recompute the size of the current set as 
					# padding will be applied to every symbol in the set
						self.recomputeSizeOnNewAlignment(symbol, newAl)

					# Compute the padding
					existingSize = symbol.getSize(arch)
					insertedSize = symbolToUpdate.getSize(arch)
					padding = 0
					while ((existingSize + insertedSize + 
						padding) % newAl) != 0:
						padding += 1

					# Set the new size and alignment, increment the ref. num
					symbol.setSize(existingSize + insertedSize + padding, arch)
					symbol.setAlignment(newAl, arch)
					symbol.addInternalSymbol(insertedSize, arch)

					#print ("New size for " + symbol.getName() + ": " +
					#	str(hex(symbol.getSize(arch))) + ", alignment=" +
					#	str(hex(symbol.getAlignment(arch))))

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
		consideredSections = symbolsList.keys()
		sectionsInfo = ReadElfParser.getSectionInfo(self.getExecutable(),
			filterSections=consideredSections)

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
			if not sectionName:  #Symbol not in one of the considered sections
				continue

			print (symbol.getName() + ": " + sectionName + " (" +
				str(hex(symbol.getAddress(0))) + ")")

			# TODO remove this check if the symbol parser does not return only
			# the symbols from the considered sections
			if sectionName not in symbolsList.keys():
				er("found a symbol from a non-considered section:\n")
				er("Section name: " + str(sectionName) + "\n")
				erStack(str(symbol) + "\n")
				sys.exit(-1)

			# is there already a symbol with that name in symbolsLists?
			currentSymbolsNames = [s.getName() for s in 
				symbolsList[sectionName]]
			if symbol.getName() not in currentSymbolsNames:
				# TODO here Chris is checking if there is no other symbol
				# with the same address and setting a flag in that symbol
				# if it is the case, it this really needed?
				symbolsList[sectionName].append(symbol)
			else:
				# Duplicate symbol for an arch, or the symbol was previously
				# added by another arch, or both
				self.updateSymbolInSymbolsList(symbol, sectionName, symbolsList)
