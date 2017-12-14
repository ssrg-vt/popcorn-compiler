''' Parse page-access trace files for various analyses.  PAT files have a line
    for each page fault recorded by the operating system at a given moment in
    the application's execution.  Each line has the following format:

    time pid perm ip addr

    Where:
      time: timestamp of fault inside of application's execution
      pid: process ID of faulting task
      perm: page access permissions
      ip: instruction address which cause the fault
      addr: faulting memory address
'''

import os
import sys
from graph import Graph

class ParseConfig:
    ''' Configuration for parsing a PAT file. Can be configuration to only
        parse entries within a given window and for certain types of accessed
        memory.
    '''
    def __init__(self, start, end, symbolTable, noCode, noData):
        self.start = start
        self.end = end
        self.symbolTable = symbolTable
        self.noCode = noCode
        self.noData = noData

def parsePAT(pat, config, callback, callbackData, verbose):
    ''' Generic parser.  For each line in the PAT file, determine if it fits
        the configuration.  If so, pass the parsed data to the callback.

        Note: we assume the entries in the PAT file are sorted by timestamp in
        increasing order.

        Arguments:
            pat (str): page access trace file
            config (ParseConfig): configuration for filtering PAT entries
            callback (func): callback function to analyze a single PAT entry
            callbackData (?): other data for the callback function
            verbose (bool): print verbose output
    '''
    if verbose: print("-> Parsing file '{}' <-".format(pat))

    with open(pat, 'r') as patfp:
        lineNum = 0
        for line in patfp:
            fields = line.split()

            # Filter based on a time window
            timestamp = float(fields[0])
            if timestamp < config.start: continue
            elif timestamp > config.end: break # No need to keep parsing

            # Filter based on type of memory object accessed
            addr = int(fields[4], base=16)
            if config.symbolTable:
                symbol = config.symbolTable.getSymbol(addr)
                if symbol:
                    if symbol.isCode() and config.noCode: continue
                    elif symbol.isData() and config.noData: continue

            if verbose:
                lineNum += 1
                if lineNum % 10000 == 0:
                    sys.stdout.write("\rParsed {} lines...".format(lineNum))
                    sys.stdout.flush()

            callback(fields, timestamp, addr, callbackData)

    if verbose: print("\rParsed {} lines".format(lineNum))

def parsePATtoGraph(pat, config, verbose):
    ''' Parse a page access trace (PAT) file and return a graph representing
        page fault patterns within a given time window.

        Arguments:
            pat (str): page access trace file
            config (ParseConfig): configuration for filtering PAT entries
            verbose (bool): print verbose output
    '''
    def graphCallback(fields, timestamp, addr, graph):
        pid = int(fields[1])
        # TODO weight read/write accesses differently?
        graph.addMapping(pid, addr & 0xfffffffffffff000)

    graph = Graph(pat, hasEdgeWeights=True)
    parsePAT(pat, config, graphCallback, graph, verbose)
    return graph

def parsePATtoTrendline(pat, config, numChunks, perthread, verbose):
    ''' Parse page fault into frequencies over the duration of the
        application's execution.  In order to graph frequencies, divide the
        execution into chunks.

        Note: in order to avoid extensive preprocessing, we assume the page
        faults are sorted by timestamp in the PAT file.

        Arguments:
            pat (str): page access trace file
            config (ParseConfig): configuration for filtering PAT entries
            numChunks (int): number of chunks into which to divide execution
            perthread (bool): maintain fault frequencies per thread
            verbose (bool): print verbose output
    '''
    def getTimeRange(pat):
        with open(pat, 'rb') as fp:
            start = float(fp.readline().split()[0])
            fp.seek(-2, os.SEEK_END)
            while fp.read(1) != b"\n": fp.seek(-2, os.SEEK_CUR)
            end = float(fp.readline().split()[0])
            return start, end

    def trendlineCallback(fields, timestamp, addr, chunkData):
        ''' Add entry to chunk bucket based on the timestamp & PID. '''
        chunks = chunkData[0]
        ranges = chunkData[1]
        curChunk = chunkData[2]

        # Move to the next chunk if the timestamp is past the upper bound of
        # the current chunk.  Note: range[i] = upper bound of chunk[i-1].
        while timestamp > ranges[curChunk]: curChunk += 1
        chunkData[2] = curChunk # Need to maintain across callbacks!

        if perthread:
            pid = int(fields[1])
            if pid not in chunks: chunks[pid] = [ 0 for i in range(numChunks) ]
            chunks[pid][curChunk] += 1
        else: chunks[curChunk] += 1

    start, end = getTimeRange(pat)
    chunkSize = (end - start) / float(numChunks)
    assert chunkSize > 0.0, "Chunk size is too small, use fewer chunks"
    if verbose: print("-> Dividing application into {} {}s-sized chunks <-" \
                      .format(numChunks, chunkSize))

    if perthread: chunks = {}
    else: chunks = [ 0 for i in range(numChunks) ]
    ranges = [ (i + 1) * chunkSize for i in range(numChunks) ]
    ranges[-1] = ranges[-1] * 1.0001 # Avoid boundary corner cases caused by FP
                                     # representation for last entry
    callbackData = [ chunks, ranges, 0 ]
    parsePAT(pat, config, trendlineCallback, callbackData, verbose)

    # Prune chunks outside of the time window.  Include chunk if at least part
    # of it is contained inside the window.
    # TODO this should be directly calculated, but we have to specially handle
    # when the user doesn't set the window start/end times
    startChunk = 0
    endChunk = 99
    for i in reversed(range(numChunks)):
        if ranges[i] >= config.start: startChunk = i
    for i in range(numChunks):
        if ranges[i] <= config.end: endChunk = i

    # Note: end number for python slices are not inclusive, but we want to
    # include endChunk so add 1
    if perthread:
        retChunks = {}
        for pid in chunks: retChunks[pid] = chunks[pid][startChunk:endChunk+1]
        return retChunks, ranges[startChunk:endChunk+1]
    else: return chunks[startChunk:endChunk+1], ranges[startChunk:endChunk+1]

def parsePATforProblemSymbols(pat, config, verbose):
    def problemSymbolCallback(fields, timestamp, addr, symData):
        objectsAccessed = symData[0]
        symbolTable = config.symbolTable

        symbol = symbolTable.getSymbol(addr)
        if symbol:
            if symbol.name not in objectsAccessed:
                objectsAccessed[symbol.name] = 0
            objectsAccessed[symbol.name] += 1
        # TODO detect heap/stack/mmap

    objectsAccessed = { "stack/mmap" : 0, "heap" : 0 }
    callbackData = [ objectsAccessed, config.symbolTable ]
    parsePAT(pat, config, problemSymbolCallback, callbackData, verbose)

    # Generate list sorted by number of items accessed
    sortedSyms = [ (objectsAccessed[sym], sym) for sym in objectsAccessed ]
    sortedSyms.sort(reverse=True, key=lambda v: v[0])
    return sortedSyms

