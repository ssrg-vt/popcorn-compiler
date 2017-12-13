'''
Parse page-access trace files for various analyses.  PAT files have a line for
each page fault recorded by the operating system at a given moment in the
application's execution.  Each line has the following format:

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

'''
Parse a page access trace (PAT) file and return a graph representing page fault
patterns within a given time window.

Arguments:
    patfile (str): page access trace file
    windowStart (float): window starting time
    windowEnd (float): window ending time
    verbose (bool): print parsing status
'''
def parsePATtoGraph(patfile, windowStart, windowEnd, verbose):
    if verbose: print("-> Parsing file '{}' <-".format(patfile))

    with open(patfile, 'r') as patfp:
        graph = Graph(patfile, hasEdgeWeights=True)
        lineNum = 0
        for line in patfp:
            fields = line.split()
            timestamp = float(fields[0])
            if timestamp >= windowStart and timestamp <= windowEnd:
                if verbose:
                    lineNum += 1
                    if lineNum % 10000 == 0:
                        sys.stdout.write("\rParsed {} lines...".format(lineNum))
                        sys.stdout.flush()

                pid = int(fields[1])
                addr = int(fields[4], base=16) & 0xfffffffffffff000

                # TODO weight read/write permissions differently?
                graph.addMapping(pid, addr)

            # Why keep parsing if we're past the end of the time window?
            if timestamp > windowEnd: break

    if verbose: print("\rParsed {} lines".format(lineNum))

    return graph

'''
Parse page fault into frequencies over the duration of the application's
execution.  In order to graph frequencies, divide execution duration into
chunks.

Note: in order to avoid extensive preprocessing, we assume the page faults are
sorted by timestamp in the PAT file.
'''
def parsePATtoTrendline(patfile, windowStart, windowEnd, perthread, verbose):
    def getTimeRange(patfile):
        with open(patfile, 'rb') as fp:
            start = float(fp.readline().split()[0])
            fp.seek(-2, os.SEEK_END)
            while fp.read(1) != b"\n": fp.seek(-2, os.SEEK_CUR)
            end = float(fp.readline().split()[0])
            return start, end

    start, end = getTimeRange(patfile)
    assert start < end, "Starting time is larger than ending time: {} vs. {}" \
        .format(start, end)
    chunkSize = (end - start) / 100.0
    assert chunkSize > 0, "Chunk size is too small"
    if verbose:
        print("-> Dividing application into {}s chunks <-".format(chunkSize))

    if perthread: chunks = {}
    else: chunks = [ 0 for i in range(100) ]
    ranges = [ (i + 1) * chunkSize for i in range(100) ]
    ranges[-1] = ranges[-1] * 1.0001 # Avoid boundary corner cases caused by FP
                                     # representation for last entry

    with open(patfile, 'r') as patfp:
        # Note: range[i] = upper bound of bucket[i-1]
        lineNum = 0
        curChunk = 0
        for line in patfp:
            fields = line.split(maxsplit=2)
            timestamp = float(fields[0])
            if timestamp >= windowStart and timestamp <= windowEnd:
                if verbose:
                    lineNum += 1
                    if lineNum % 10000 == 0:
                        sys.stdout.write("\rParsed {} lines...".format(lineNum))
                        sys.stdout.flush()

                # Move to next chunk if the timestamp is past the upper time
                # bound of the current chunk
                if timestamp > ranges[curChunk]: curChunk += 1

                # TODO do per-thread
                if perthread:
                    pid = int(fields[1])
                    if pid not in chunks:
                        chunks[pid] = [ 0 for i in range(100) ]
                    chunks[pid][curChunk] += 1
                else: chunks[curChunk] += 1

            # Why keep parsing if we're past the end of the time window?
            if timestamp > windowEnd: break

    if verbose: print("\rParsed {} lines".format(lineNum))

    # Prune chunks outside of the time window.  Include chunk if at least part
    # of it is contained inside the window.
    # TODO this should be directly calculated, but we have to specially handle
    # when the user doesn't set the window start/end times
    startChunk = 0
    endChunk = 99
    for i in reversed(range(100)):
        if ranges[i] >= windowStart: startChunk = i
    for i in range(100):
        if ranges[i] <= windowEnd: endChunk = i

    # Note: end number for python slices are not inclusive, but we want to
    # include endChunk so add 1
    if perthread:
        retChunks = {}
        for pid in chunks: retChunks[pid] = chunks[pid][startChunk:endChunk+1]
        return retChunks, ranges[startChunk:endChunk+1]
    else:
        return chunks[startChunk:endChunk+1], ranges[startChunk:endChunk+1]

