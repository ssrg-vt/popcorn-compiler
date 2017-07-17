from Arch import Arch, Archs
from Utils import readElf
from MapParser import parseMapFile

# TODO remove this when done
if __name__ == "__main__":
	print "hello"	

	symbols = parseMapFile("map_x86.txt")
	for index1, symbol1 in enumerate(symbols):
		for index2, symbol2 in enumerate(symbols):
			if index1 != index2 and symbol1.getName() == symbol2.getName():
				print "Same symbols:"
				print symbol1
				print symbol2
				print "-----------------------"

#	print Archs[Arch.X86].getLibGccLocation()
#	print Archs[Arch.POWER].getLibGccLocation()
#	print Archs[Arch.ARM].getLibGccLocation()

#	readElf("/root/popcorn-compiler-arm-x86/pierre-test/pierre.o", 
#		"/root/test")

#	Archs[Arch.X86].goldLink(["/root/popcorn-compiler-arm-x86/pierre-test/pierre_x86_64.o", "/root/popcorn-compiler-arm-x86/pierre-test/test_x86_64.o"])
##	Archs[Arch.ARM].goldLink(["/root/popcorn-compiler-arm-x86/pierre-test/pierre.o", "/root/popcorn-compiler-arm-x86/pierre-test/test.o"])
