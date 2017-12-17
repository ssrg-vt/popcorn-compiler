"""
We put everything common to each in this class which act as an abstract class.
Basically, this contains evething that is done per-architecture, but is done
relatively the same way for each architecture.
"""

import subprocess, sys, os, re
import Globals, ReadElfParser, Symbol
from Globals import er, warn

class AbstractArchitecture():

	def getLinkerScript(self):
		return self._linkerScript

	def setLinkerScript(self, ls):
		self._linkerScript = ls

	def getLsTemplate(self):
		return (Globals.POPCORN_LOCATION + "/share/align-script-templates/" + 
			self._linkerScriptTemplate)

	def getExecutable(self):
		return self._executable

	def setExecutable(self, executable):
		self._executable = executable

	def getMapFile(self):
		return self._mapFile

	def setMapFile(self, mapFile):
		self._mapFile = mapFile

	def parseMapFile(self):
		"""Returns a list of Symbols instances extracted from the map file which
		path is filePath (should have been generalted by gold.ld -Map <file>
		"""
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

	def getSection(self, symbol, sectionInfo):
		"""Return the section associated with a symbol as a string (ex: ".text")
		symbol is a Symbol instance, sectionInfo is a list of Section object,
		the	list that is returned by ReadElfParser.getSectionInfo()
		"""
		arch = self.getArch()
		addr = symbol.getAddress(arch)
		res = None

		for section in sectionInfo:
			sectionAddr = section.getAddress()
			sectionSize = section.getSize()
			sectionName = section.getName()
			if symbol.getName().startswith(sectionName):
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

			#FIXME: Since the TLS fix, the hack below should probabaly be removed

			# Super special case, I have seen this in some map files, don't
			# really know what it means ...
			if (addr == (sectionAddr + sectionSize)):
				if symbol.getName().startswith(sectionName): #check name
					res = sectionName
					break

		if not res:
			pass

		return res

	def updateSymbolsList(self, symbolsList):
		"""symbolsList is a dictionary of lists, one per section, for example:
		symbolsList = { ".text" : [], ".rodata" : [], ".bss" : [], ".data" : [],
		".tdata" : [], ".tbss" : [] }
		This function TODO update description and maybe change the name
		accordinglyn. The way to call it is:
		Arch1.updateSymbolsList(list)
		Arch2.updateSymbolsList(list)
		Arch3.updateSymbolsList(list)
		etc.
		"""
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
