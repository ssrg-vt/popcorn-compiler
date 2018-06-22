#!/usr/bin/python3

import sys, subprocess, os, re

###############################################################################
# Config
###############################################################################

binA = None
binB = None
verbose = False
continueCheck = True
consideredSections = [".text", ".data", ".rodata", ".bss", ".init"]

###############################################################################
# Utility functions
###############################################################################

def printHelp():
	print("check-align.py: ensure alignment tool aligned everything\n")

	print("Usage: ./check-align.py <binary A> <binary B> [ OPTIONS ]")
	print("Options:")
	print("  -h / --help : print help & exit")
	print("  -v          : verbose output")
	print("  -s          : stop after detecting mis-alignment")

# This function takes a path to an ELF binary as parameter, executes readelf
# on it, parsing the output, building then returning a list of sections
def getSectionInfo(binaryPath):
	absolutePath = os.path.abspath(binaryPath)
	cmd = ["readelf", "-SW", absolutePath]
	res = []
	readelfRe = ("^[\s]*\[([\s0-9]+)\]\s([.\S]*)?\s+([.\S]+)\s+([0-9a-f]+)" +
		"\s+([0-9a-f]+)\s+([0-9a-f]+)\s+([0-9a-f]+)\s+([.\S]*)\s+([0-9a-f]+)" +
		"\s+([0-9a-f]+)\s+([0-9a-f]+)$")

	try:
		readelfOutput = subprocess.check_output(cmd,
			stderr=subprocess.STDOUT)
	except subprocess.CalledProcessError as e:
		print("Error: executing readelf " + absolutePath + " :")
		er(e.output)
		sys.exit(-1)

	for line in readelfOutput.decode().split("\n"):
		matchRes = re.match(readelfRe, line)
		if matchRes :
			if matchRes.group(2) in consideredSections:
				s = {
					"index": int(matchRes.group(1)),
					"name": matchRes.group(2),
					"address": int("0x" + matchRes.group(4), 0),
					"size": int("0x" + matchRes.group(6), 0)
				}
				res.append(s)

	return res

def checkSections(sectionInfoA, sectionInfoB):

        for sectionA in sectionInfoA:
                flag = False
                for sectionB in sectionInfoB:
                        if sectionA["name"] == sectionB["name"]:
                                flag = True
                                if sectionA["address"] != sectionB["address"]:
                                        print("Error: " + sectionA["name"] + "does not have simillar Address")
                                        return
                if flag == False:
                        print("Error: " + sectionA["name"] + "is not found in Binary B")
                        return
###############################################################################
# Driver
###############################################################################

if len(sys.argv) < 3:
	if len(sys.argv) == 2 and (sys.argv[1] == "-h" or sys.argv[1] == "--help"):
		retCode = 0
	else:
		print("Error: please supply 2 binaries!")
		retCode = 1
	printHelp()
	sys.exit(retCode)

binA = sys.argv[1]
binB = sys.argv[2]

skip = False
for i in range(len(sys.argv)):
	if skip:
		skip = False
		continue
	elif sys.argv[i] == "-h" or sys.argv[i] == "--help":
		printHelp()
		sys.exit(0)
	elif sys.argv[i] == "-v":
		verbose = True
		continue
	elif sys.argv[i] == "-s":
		continueCheck = False
		continue

sectionInfoA = getSectionInfo(binA)
sectionInfoB = getSectionInfo(binB)
sys.exit(checkSections(sectionInfoA, sectionInfoB))
