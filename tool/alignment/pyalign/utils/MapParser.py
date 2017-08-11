import Symbol
import re

def parseMapFile(filePath, arch):
	res = []
	twoLinesRe1 = "^[\s]+(\.[texrodalcbs\.]+[\S]*)$"
	twoLinesRe2 = ("^[\s]+(0x[0-9a-f]+)[\s]+(0x[0-9a-f]+)[\s]+" + 
		"(0x[0-9a-f]+)[\s]+(.*)$")
	oneLineRe = ("^[\s]+(\.[texrodalcbs\.]+[\S]+)[\s]+(0x[0-9a-f]+)[\s]+" + 
		"(0x[0-9a-f]+)[\s]+(0x[0-9a-f]+)[\s]+(.*)$")

	shittyFlag = 0

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
					objectFile, arch)
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
						objectFile, arch)
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

