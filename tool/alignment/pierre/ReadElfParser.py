import sys
import subprocess
import re
import os

# Represents a section. Each attribute is one of the fields reported by 
# readelf -S
class Section:
	_index = -1
	_name = ""
	_secType = ""
	_address = -1
	_offset = -1
	_size = -1
	_es = -1   # entry size if section holds table
	_flags = ""
	_lk = -1   # index of another section
	_inf = -1  # additional section information
	_alignment = -1

	def __init__(self, index, name, secType, address, offset, size, es, flags,
			lk, inf, alignment):
		self._index = index
		self._name = name
		self._secType = secType
		self._address = address
		self._offset = offset
		self._size = size
		self._es = es
		self._flags = flags
		self._lk = lk
		self._inf = inf
		self._alignment = alignment

	def __str__(self):
		res = ("Section, index=" + str(self.getIndex()) +
			", name=" + self.getName() + ", secType=" + self.getSecType() +
			", address=" + str(self.getAddress()) +", offset=" + 
			str(self.getOffset()) +	", size=" + str(self.getSize()) + 
			", es=" + str(self.getEs()) + ", flags=" + self.getFlags() + 
			", lk=" + str(self.getLk()) + ", inf=" + str(self.getInf()) + 
			", alignment=" + str(self.getAlignment()))
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

# FIXME: do we need setters here?
#	def setIndex(self, index):
#		self._index = index
#
#	def setName(self, name):
#		self._name = name
#
#	def setSecType(self, secType):
#		self._secType = secType
#
#	def setAddress(self, address):
#		self._address = address
#
#	def setOffset(self, offset):
#		self._offset = offset
#
#	def setSize(self, size):
#		self._size = size
#
#	def setEs(self, es):
#		self._es = es
#
#	def setFlags(self, flags):
#		self._flags = flags
#
#	def setLk(self, lk):
#		self._lk = lk
#
#	def setInf(self, inf):
#		self._inf = inf
#
#	def setAlignment(self, alignment):
#		self._alignment = alignment

	# Returns true if a given address is located inside the address range 
	# corresponding to this section
	# FIXME is this needed?
	def checkAddressInSection(self, address):
		return (address >= self.getAddress() and 
			address < (self.getAddress() + self.getSize()))

# This function takes a path to an ELF binary as parameter, executes readelf
# on it, parsing the output, building then returning a list of sections 
# objects
def getSectionInfo(binaryPath):
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
		sys.stderr.write("ERROR: executing readelf " + absolutePath + 
			" :\n")
		sys.stderr.write(e.output)
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
			res.append(s)

	return res
