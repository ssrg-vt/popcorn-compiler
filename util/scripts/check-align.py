#!/usr/bin/python3

import sys, subprocess, os, re

###############################################################################
# Config
###############################################################################

binA = None
binB = None
binC = None
verbose = False
continueCheck = True
consideredSections = [".text", ".data", ".rodata", ".bss", ".tdata", ".tbss",
	".ldata", ".lrodata", ".lbss", ".init", ".fini", ".init_array",
	".fini_array", ".ctors", ".dtors"]

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
		if matchRes:
			s = {
				"index": int(matchRes.group(1)),
				"name": matchRes.group(2),
				"address": int("0x" + matchRes.group(4), 0),
				"size": int("0x" + matchRes.group(6), 0)
			}
			res.append(s)

	return res

# Check if an address falls into one of the considered sections
# sectionInfo is a dictionary as returned by getSectionInfo
def addrInConsideredSec(addr, sectionInfo):
	for section in sectionInfo:
		if section["name"] in consideredSections:
			secStart = section["address"]
			secEnd = section["address"] + section["size"]
			if (addr >= secStart) and (addr < secEnd):
				return True
	return False

def getSymbols(binFile):
	symbols = {}
	out = subprocess.check_output(["nm", "-v", binFile]) # sort symbols by @
	outlines = out.decode("utf-8").split("\n")
	for line in outlines:
		toks = line.strip().split()
		if len(toks) < 3:
			continue
		symbol = toks[2]
		# We can have multiple symbols with the same name, rename it
		if symbol in symbols.keys():
			suffix = 1
			while ((symbol + "_" + str(suffix)) in symbols.keys()):
				suffix += 1
			symbol = symbol + "_" + str(suffix)

		symbols[symbol] = (int(toks[0], base=16), toks[1])
	return symbols

def checkSymbols(symbolsA, symbolsB, symbolsC, sectionInfo):
	global verbose
	global continueCheck
	retCode = 0

	for symbol in sorted(symbolsA):
		if not addrInConsideredSec(symbolsA[symbol][0], sectionInfo):
			continue
		if symbol not in symbolsB:
			if verbose:
				print("Warning: found '" + symbol + "' in " + binA + " but not in " + binB)
			continue
		if symbolsC and symbol not in symbolsC:
			if verbose:
				print("Warning: found '" + symbol + "' in " + binA + " but not in " + binC)
			continue

		else:
			if symbolsA[symbol][0] != symbolsB[symbol][0]:
				print("Error: '" + symbol + "' (" + symbolsA[symbol][1] + \
							") not aligned (" + hex(symbolsA[symbol][0]) + " vs. " + \
							hex(symbolsB[symbol][0]) + ")")
				retCode = 1
				if not continueCheck:
					break
			if symbolsC and symbolsA[symbol][0] != symbolsC[symbol][0]:
				print("Error: '" + symbol + "' (" + symbolsA[symbol][1] + \
							") not aligned (" + hex(symbolsA[symbol][0]) + " vs. " + \
							hex(symbolsC[symbol][0]) + ")")
				retCode = 1
				if not continueCheck:
					break

			elif verbose:
				print("'" + symbol + "' (" + symbolsA[symbol][1] + ") aligned @ " + \
							hex(symbolsA[symbol][0]))
	return retCode

###############################################################################
# Driver
###############################################################################

if len(sys.argv) < 3:
	if len(sys.argv) == 2 and (sys.argv[1] == "-h" or sys.argv[1] == "--help"):
		retCode = 0
	else:
		print("Error: please supply 2 or 3 binaries!")
		retCode = 1
	printHelp()
	sys.exit(retCode)

binA = sys.argv[1]
binB = sys.argv[2]
if(len(sys.argv) == 4):
    binC = sys.argv[3]

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

symbolsA = getSymbols(binA)
symbolsB = getSymbols(binB)
symbolsC = {}
if binC:
    symbolsC = getSymbols(binC)
sectionInfo = getSectionInfo(binA)
sys.exit(checkSymbols(symbolsA, symbolsB, symbolsC, sectionInfo))

