from Arch import Arch

class Linker:

	_sectionmarker = {	".text" 	: "__TEXT__",
						".data" 	: "__DATA__",
						".bss"  	: "__BSS__",
						".rodata" 	: "__RODATA__",
						".tdata" 	: "__TDATA__",
						".tbss" 	: "__TBSS__" }

	@classmethod
	def getSectionMarker(cls, sectionName):
		return cls._sectionmarker[sectionName]

	@classmethod
	def setSectionMarker(cls, sectionName, sectionMarker):
		cls._sectionmarker[sectioName] = sectionMarker

	# symbolLists should be a dictionary of per-section Symbol lists, ex:
	# { ".text" : [S1, S2, etc.], ".data" : [S1, S2, etc.], etc. }
	# arch is an Arch instance
	@classmethod
	def produceLinkerScript(cls, symbolsList, arch):
		template = arch.getLsTemplate()
	
		with open(template, "r") as f:
			lines = f.readlines()
		print lines
