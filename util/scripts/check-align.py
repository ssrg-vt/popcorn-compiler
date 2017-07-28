#!/usr/bin/python3

import sys, subprocess

###############################################################################
# Config
###############################################################################

binA = None
binB = None
verbose = False
continueCheck = True

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

def getSymbols(binFile):
	symbols = {}
	out = subprocess.check_output(["nm", binFile])
	outlines = out.decode("utf-8").split("\n")
	for line in outlines:
		toks = line.strip().split()
		if len(toks) < 3:
			continue
		symbol = toks[2]
		symbols[symbol] = (int(toks[0], base=16), toks[1])
	return symbols

def checkSymbols(symbolsA, symbolsB):
	global verbose
	global continueCheck
	retCode = 0

	for symbol in sorted(symbolsA):
		if symbol not in symbolsB:
			if verbose:
				print("Warning: found '" + symbol + "' in binary A but not binary B")
			continue
		else:
			if symbolsA[symbol][0] != symbolsB[symbol][0]:
				print("Error: '" + symbol + "' (" + symbolsA[symbol][1] + \
							") not aligned (" + hex(symbolsA[symbol][0]) + " vs. " + \
							hex(symbolsB[symbol][0]) + ")")
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

symbolsA = getSymbols(binA)
symbolsB = getSymbols(binB)
sys.exit(checkSymbols(symbolsA, symbolsB))

