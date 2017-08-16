"""
Extracts elf sections info from a binary, calls readelf -S under the hood
"""

import sys, subprocess, re, os
from Globals import er

class Section:
	"""Represents a section. Each attribute is one of the fields reported by 
	readelf -S
	"""

	def __init__(self, index, name, secType, address, offset, size, es, flags,
			lk, inf, alignment):
		self._index = index
		self._name = name
		self._secType = secType
		self._address = address
		self._offset = offset
		self._size = size
		self._es = es		# entry size if section holds table
		self._flags = flags
		self._lk = lk		# index of another section
		self._inf = inf		# additional section information
		self._alignment = alignment

	def __str__(self):
		res = ("Section, index=" + str(self.getIndex()) +
			", name=" + self.getName() + ", secType=" + self.getSecType() +
			", address=" + str(hex(self.getAddress())) +", offset=" + 
			str(hex(self.getOffset())) +	", size=" + 
			str(hex(self.getSize())) + 	", es=" + str(hex(self.getEs())) + 
			", flags=" + self.getFlags() + ", lk=" + str(self.getLk()) + 
			", inf=" + str(self.getInf()) + ", alignment=" + 
			str(self.getAlignment()))
		return res

	def getIndex(self):
		return self._index

	def getName(self):
		return self._name

	def getSecType(self):
		return self._secType

	def getAddress(self):
		return self._address

	def getOffset(self):
		return self._offset

	def getSize(self):
		return self._size

	def getEs(self):
		return self._es

	def getFlags(self):
		return self._flags
	
	def getLk(self):
		return self._lk

	def getInf(self):
		return self._inf

	def getAlignment(self):
		return self._alignment

def getSectionInfo(binaryPath, filterSections=None):
	"""This function takes a path to an ELF binary as parameter, executes 
	readelf on it, parsing the output, building then returning a list of 
	sections objects
	filterSections is a list of sections to consider (i.e. the result returned
	will only contain info about these), ex: [".data", ".text", etc.]
	"""
	absolutePath = os.path.abspath(binaryPath)
	cmd = ["readelf", "-SW", absolutePath]
	res = []
	readelfRe = ("^[\s]*\[([\s0-9]+)\]\s([.\S]*)?\s+([.\S]+)\s+([0-9a-f]+)" + 
		"\s+([0-9a-f]+)\s+([0-9a-f]+)\s+([0-9a-f]+)\s+([.\S]*)\s+([0-9a-f]+)" +
		"\s+([0-9a-f]+)\s+([0-9a-f]+)$") # not bad :)	

	try:
		readelf_output = subprocess.check_output(cmd,
			stderr=subprocess.STDOUT)
	except subprocess.CalledProcessError as e:
		er("executing readelf " + absolutePath + " :\n")
		er(e.output)
		sys.exit()

	for line in readelf_output.split("\n"):
		matchRes = re.match(readelfRe, line)
		if matchRes:
			# I checked readelf sources so I know which of these are in hex
			# and which are in decimal format
			s = Section(int(matchRes.group(1)),		# index 	(dec)
				matchRes.group(2), 					# name
				matchRes.group(3),					# type
				int("0x" + matchRes.group(4), 0), 	# address	(hex)
				int("0x" + matchRes.group(5), 0),	# offset	(hex)
				int("0x" + matchRes.group(6), 0),	# size		(hex)
				int("0x" + matchRes.group(7), 0), 	# ES		(hex)
				matchRes.group(8), 					# flags
				int(matchRes.group(9)),				# Lk
				int(matchRes.group(10)), 			# Inf
				int(matchRes.group(11)))			# alignment
			
			if filterSections and (s.getName() not in filterSections):
				continue

			res.append(s)

	return res
