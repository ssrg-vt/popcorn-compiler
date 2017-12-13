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
                perm = fields[2]
                ip = int(fields[3], base=16)
                addr = int(fields[4], base=16) & 0xfffffffffffff000

                # TODO weight read/write permissions differently?
                graph.addMapping(pid, addr)

            # Why keep parsing if we're past the end of the time window?
            if timestamp > windowEnd: break

    if verbose: print("\rParsed {} lines".format(lineNum))

    return graph

