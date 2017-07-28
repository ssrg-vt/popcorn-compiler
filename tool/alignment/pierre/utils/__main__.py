import sys
import MapParser
import Symbol
from Arch import Arch
from Globals import er

# These are the symols with multiple occurence, TODO for later
blacklist = [".text", ".text.byte_copy", ".text.cleanup", ".text.dummy"]

def printHelp(argv):
	print "Usage: %s <x86_map> <arm_map>" % (argv[0])

if __name__ == "__main__":
	if len(sys.argv) != 3:
		printHelp(sys.argv)

	x86Map = sys.argv[1]
	armMap = sys.argv[2]

	x86Symbols = MapParser.parseMapFile(x86Map, Arch.X86)
	armSymbols = MapParser.parseMapFile(armMap, Arch.ARM)

	print "Found " + str(len(x86Symbols)) + " x86 symbols"
	print "Found " + str(len(armSymbols)) + " ARM symbols"

	for symbolX86 in x86Symbols:
		name = symbolX86.getName()
		if name == ".text.gelf_msize":
			print "coucou"
		if name in blacklist:
			continue
		for symbolArm in armSymbols:
			if name == symbolArm.getName():
				addrX86 = symbolX86.getAddress(Arch.X86)
				addrArm = symbolArm.getAddress(Arch.ARM)
				if addrX86 != addrArm:
					er("Unaligned symbol: " + name + "\n")
					er("X86 address: " + str(hex(addrX86)) + "\n")
					er("ARM address: " + str(hex(addrArm)) + "\n")
					sys.exit()
