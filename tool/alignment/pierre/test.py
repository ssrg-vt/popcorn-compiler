from Arch import Arch, Archs

# TODO remove this when done
if __name__ == "__main__":
	print "hello"	

	print Archs[Arch.SPARC].getLibGccLocation()
	print Archs[Arch.X86].getLibGccLocation()
	print Archs[Arch.POWER].getLibGccLocation()
	print Archs[Arch.ARM].getLibGccLocation()

	Archs[Arch.X86].goldLink(["/root/popcorn-compiler-arm-x86/pierre-test/pierre_x86_64.o", "/root/popcorn-compiler-arm-x86/pierre-test/test_x86_64.o"])
	Archs[Arch.ARM].goldLink(["/root/popcorn-compiler-arm-x86/pierre-test/pierre.o", "/root/popcorn-compiler-arm-x86/pierre-test/test.o"])
