''' Parse page-access trace files for various analyses.  PAT files have a line
    for each page fault recorded by the operating system at a given moment in
    the application's execution.  Each line has the following format:

    time nid pid perm ip addr

    Where:
      time: timestamp of fault inside of application's execution
      nid : ID of node on which fault occurred
      pid: process ID of faulting task
      perm: page access permissions
      ip: instruction address which cause the fault
      addr: faulting memory address
      region: region identifier
'''

import os
import sys
from graph import Graph

def getPage(addr):
    ''' Get the page for an address '''
    return addr & 0xfffffffffffff000

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
            addr = int(fields[5], base=16)
            if config.symbolTable:
                symbol = config.symbolTable.getSymbol(addr)
                if symbol:
                    if symbol.isCode() and config.noCode: continue
                    elif symbol.isData() and config.noData: continue
            else: symbol = None

            if verbose:
                lineNum += 1
                if lineNum % 10000 == 0:
                    sys.stdout.write("\rParsed {} faults...".format(lineNum))
                    sys.stdout.flush()

            callback(fields, timestamp, addr, symbol, callbackData)

    if verbose: print("\rParsed {} faults".format(lineNum))

def parsePATtoGraphs(pat, config, verbose):
    ''' Parse a page access trace (PAT) file and return a graph representing
        page fault patterns within a given time window.

        Arguments:
            pat (str): page access trace file
            config (ParseConfig): configuration for filtering PAT entries
            verbose (bool): print verbose output

        Return:
            graph (Graph): graph containing thread -> page mappings
    '''
    def graphCallback(fields, timestamp, addr, symbol, graphData):
        graphs = graphData[0]
        region = int(fields[6])
        pid = int(fields[2])
        if region not in graphs: graphs[region] = Graph(graphData[1], True)
        # TODO weight read/write accesses differently?
        graphs[region].addMapping(pid, getPage(addr))

    graphs = {}
    callbackData = (graphs, pat)
    parsePAT(pat, config, graphCallback, callbackData, verbose)

    if verbose:
        regions = ""
        for region in graphs: regions += "{} ".format(region)
        print("Found {} regions: {}".format(len(graphs), regions[:-1]))

    return graphs

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

        Return:
            chunks (list:int): list of page faults within a chunk
            ranges (list:float): upper time bound of each chunk

        Return (per-thread):
            chunks (dict: int -> list:int): per-thread page faults within a
                                            chunk
            ranges (list:float): upper time bound of each chunk
    '''
    def getTimeRange(pat):
        with open(pat, 'rb') as fp:
            start = float(fp.readline().split()[0])
            fp.seek(-2, os.SEEK_END)
            while fp.read(1) != b"\n": fp.seek(-2, os.SEEK_CUR)
            end = float(fp.readline().split()[0])
            return start, end

    def trendlineCallback(fields, timestamp, addr, symbol, chunkData):
        ''' Add entry to chunk bucket based on the timestamp & PID. '''
        chunks = chunkData[0]
        ranges = chunkData[1]
        curChunk = chunkData[2]

        # Move to the next chunk if the timestamp is past the upper bound of
        # the current chunk.
        while timestamp > ranges[curChunk]: curChunk += 1
        chunkData[2] = curChunk # Need to maintain across callbacks!

        if perthread:
            pid = int(fields[2])
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
    ranges = [ (i + 1) * chunkSize + start for i in range(numChunks) ]
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
    ''' Parse PAT for symbols which cause the most faults.  Return a list of
        symbols sorted by the highest number of faults.

        Arguments:
            pat (str): page access trace file
            config (ParseConfig): configuration for filtering PAT entries
            verbose (bool): print verbose output

        Return:
            sortedSyms (list:tup(int, Symbol)): list of symbols sorted by how
                                                often they're accessed, in
                                                descending order
    '''

    def problemSymbolCallback(fields, timestamp, addr, symbol, objAccessed):
        if symbol:
            if symbol.name not in objAccessed: objAccessed[symbol.name] = 0
            objAccessed[symbol.name] += 1
        else:
            # TODO this is only an approximation!
            if addr > 0x7f0000000000: objAccessed["stack/mmap"] += 1
            else: objAccessed["heap"] += 1

    objAccessed = { "stack/mmap" : 0, "heap" : 0 }
    parsePAT(pat, config, problemSymbolCallback, objAccessed, verbose)

    # Generate list sorted by number of times accessed
    sortedSyms = [ (objAccessed[sym], sym) for sym in objAccessed ]
    sortedSyms.sort(reverse=True, key=lambda v: v[0])
    return sortedSyms

def parsePATforFalseSharing(pat, config, verbose):
    ''' Parse PAT for symbols which induce false sharing, i.e., two symbols
        on the same page accessed by threads on multiple nodes with R/W or
        W/W permissions.

        Arguments:
            pat (str): page access trace file
            config (ParseConfig): configuration for filtering PAT entries
            verbose (bool): print verbose output
    '''

    class PageTracker:
        ''' Logically track accesses to a page, including faults caused by
            false sharing.  False sharing is, by definition, caused when
            faults are induced by accessing separate program objects mapped to
            the same page.
        '''
        def __init__(self, page):
            self.page = page
            self.faults = 0
            self.falseFaults = 0
            self.seen = set([0])
            self.hasCopy = set([0])
            self.lastSym = None
            self.problemSymbols = set()

        def track(self, symbol, nid, perm):
            ''' Track whether this fault is due to false sharing '''
            self.faults += 1

            # The first page access is not considered false sharing, as the
            # first fault happens regardless of any consistency protocol (we
            # have to transport the data over at least once!)
            if nid not in self.seen:
                self.seen.add(nid)
                return

            if perm == "W":
                # Either we're upgrading an existing page's permissions from
                # "R", or we're in an invalid state due to a previous write;
                # check if it was to the same symbol.
                if nid not in self.hasCopy and \
                        self.lastSym and symbol != self.lastSym:
                    self.problemSymbols.add(symbol)
                    self.problemSymbols.add(self.lastSym)
                    self.falseFaults += 1
                self.hasCopy = set([nid])
                self.lastSym = symbol
            else: # perm == "R"
                # We're in the invalid state (we never need to "downgrade"
                # permissions) due to a previous write; check if was to the
                # same symbol.
                if self.lastSym and symbol != self.lastSym:
                    self.problemSymbols.add(symbol)
                    self.problemSymbols.add(self.lastSym)
                    self.falseFaults += 1
                self.hasCopy.add(nid)

    def falseSharingCallback(fields, timestamp, addr, symbol, pagesAccessed):
        # Note: we can only track symbol table values, as we don't know the
        # semantics of the stack/heap/mapped memory
        if symbol:
            page = getPage(addr)
            if page not in pagesAccessed: pagesAccessed[page] = PageTracker(page)
            pagesAccessed[page].track(symbol.name, int(fields[1]), fields[3])

    pagesAccessed = {}
    parsePAT(pat, config, falseSharingCallback, pagesAccessed, verbose)

    return sorted(pagesAccessed.values(),
                  reverse=True,
                  key=lambda p: p.falseFaults)

