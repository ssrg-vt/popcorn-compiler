''' Implement the API to read & write graph files according to METIS' graph
    format.
'''

import os
from os import path
import sys
import graph
from graph import Graph
import random
import subprocess

prefix = "/tmp/place-threads-"

###############################################################################
# Printing
###############################################################################

def writeReadme(region, graph, gpmetis, nodes, directory, suffix):
    ''' If saving intermediate information, describe how it was generated. '''
    with open(directory + "/README", 'w') as fp:
        fp.write("Partitioning generated from page access trace file '{}'\n" \
                 .format(graph.patFile))
        fp.write("  - Partitioning for region {}\n".format(region))
        fp.write("  - Partitioning using '{}'\n".format(gpmetis))
        fp.write("  - Distributing threads across {} nodes\n\n".format(nodes))
        fp.write("METIS graph file: place-threads-{}.graph\n".format(suffix))
        fp.write("Partitioning result: place-threads-{}.graph.part.{}\n" \
                 .format(suffix, nodes))
        fp.write("METIS output: place-threads-{}.metis.out\n".format(suffix))

def writeGraphAsMatrix(directory, graph):
    ''' Write the graph as an adjacency matrix in nice formatting '''
    def getLargest(matrix, labels):
        largest = 0
        for row in matrix:
            for col in row:
                length = len(str(col))
                if length > largest: largest = length

        for label in labels:
            length = len(str(label))
            if length > largest: largest = length

        return largest

    matrix, labels = graph.getAdjacencyMatrix()
    width = getLargest(matrix, labels)
    with open(directory + "/matrix.graph", 'w') as fp:
        # First row is just labels
        fp.write("{l:>{w}} ".format(l=" ", w=width))
        for label in labels:
            fp.write(" {l:>{w}} ".format(l=label, w=width))
        fp.write("\n\n")

        cur = 0
        for row in matrix:
            fp.write("{l:<{w}} ".format(l=labels[cur], w=width))
            for col in row:
                fp.write(" {l:>{w}} ".format(l=col, w=width))
            fp.write("\n\n")
            cur += 1

def getHeader(graph):
    ''' Get the header for the graph file which contains configuration
        information.
    '''
    def getFormat(graph):
        def toBinary(flag):
            if flag: return "1"
            else: return "0"

        val = toBinary(graph.hasVertexSizes)
        val += toBinary(graph.numVertexWeights > 0)
        val += toBinary(graph.hasEdgeWeights)
        return val

    return "{} {} {} {}".format(graph.getNumVertices(),
                                graph.getNumEdges(),
                                getFormat(graph),
                                graph.numVertexWeights)

def getVertexComment(vertex, idx):
    return "% index {} -- {}".format(idx, vertex)

def getVertexData(vertex, indexes):
    assert len(vertex.edges) > 0, "No edges for {}".format(vertex)
    ret = ""
    for other in sorted(vertex.edges):
        ret += "{} {} ".format(indexes[other], vertex[other])
    return ret[:-1]

def writeGraphToFile(graph, ptids, suffix, verbose):
    ''' Write a METIS-formatted graph file.  For a graph G = (V, E) with
        n = |V| vertices and m = |E| edges, the file has n+1 lines.  The first
        line is a header describing the graph configuration, and the remaining
        n lines are adjacency lists for each vertex in the graph.  The file has
        the following format

        n m fmt nvweights
        <edge 1> <edge weight 1> <edge 2> <edge weight 2> ...
        ...

        Header:
        - The fmt parameter is a 3-digit binary number describing the following
          characteristics (from LSB to MSB):
            - The graph has edge weights
            - The graph has vertex weights
            - The graph has vertex sizes
        - The nvweights parameter describes how many vertex weights are
          associated with each vertex in the graph.  If nvweights > 0, then bit
          2 of fmt must be set.

        Vertices:
        - Each line after the header represents an adjacency list for a vertex
          in the graph.  Adjacency lists are maintained in <index, weight>
          tuples.
        - Other vertices are referenced by their location in the graph file,
          e.g., vertex 1 is on line 1, vertex 40 is on line 40, etc.
        - Commented lines begin with '%' and do not affect the indexes of
          vertices.

        See the METIS manual for more information:
        http://glaros.dtc.umn.edu/gkhome/fetch/sw/metis/manual.pdf
    '''
    global prefix
    graphfile = prefix + suffix + ".graph"
    if verbose: print("-> Printing METIS graph file '{}' <-".format(graphfile))

    with open(graphfile, 'w') as out:
        # We reference other vertices by their index in the graph file, so
        # assign an ordering to all TIDs & pages

        if ptids: # Order threads by the mapping
            mapping = sorted(ptids, key=lambda tup:tup[1])
            vertices = []
            for pair in mapping:
                # It's possible that we don't see a thread in the mapping file
                # cause a page fault in this graph
                if pair[0] in graph.tids: vertices.append(graph.tids[pair[0]])
                else: vertices.append(graph.addEmptyTID(pair[0]))

            graphtids = set(graph.tids.keys())
            mapfiletids = set([ t[0] for t in ptids ])
            diff = graphtids.difference(mapfiletids)
            if len(diff) != 0:
                for tid in diff:
                    print("Missing TID {} from mapping file".format(tid))
                assert False, "Mapping file doesn't cover all TIDs!"
        else: vertices = sorted(graph.tids.values()) # Sort by increasing TID
        vertices += sorted(graph.pages.values())

        indexes = {}
        idx = 1
        for vertex in vertices:
            indexes[vertex.name] = idx
            idx += 1

        # First, write the header
        out.write(getHeader(graph) + "\n")

        # Next, write vertices to file
        for vertex in vertices:
            if verbose:
                out.write(getVertexComment(vertex, indexes[vertex.name]) + "\n")
            out.write(getVertexData(vertex, indexes) + "\n")

    if verbose:
        print("Printed graph with {} vertices and {} edges" \
              .format(graph.getNumVertices(), graph.getNumEdges()))

    # Return the graph file & indexes for parsing the resulting partitioning
    return graphfile, { indexes[k] : k for k in graph.tids }

def writeThreadPlacements(schedule, region, indexes, partitioning, verbose):
    ''' Write thread schedule file based on which nodes METIS thinks we should
        place threads.
    '''
    threads = sorted(indexes.items(), key=lambda tup:tup[0])
    nodes = {}
    with open(partitioning, 'r') as partition:
        for thread in threads:
            node = partition.readline().strip()
            if verbose:
                print("Thread {} (TID: {}) should be placed on node {}" \
                      .format(thread[0] - 1, thread[1], node))
            nodes[thread[0]] = node

    with open(schedule, 'a') as schedfp:
        schedfp.write("{} {}".format(region, len(nodes)))
        for ptid in sorted(nodes.keys()):
            schedfp.write(" {}".format(nodes[ptid]))
        schedfp.write("\n")

###############################################################################
# Parsing
###############################################################################

def parseTIDMapFile(mapFile, verbose):
    # Format: one mapping per line
    #   <Linux TID> <Popcorn ID>
    try:
        with open(mapFile, 'r') as fp:
            if verbose:
                print("-> Parsing thread mapping file '{}' <-".format(mapFile))
            ids = []
            for line in fp:
                fields = line.split()
                ids.append((int(fields[0]), int(fields[1])))
            return ids
    except Exception as e:
        return None

###############################################################################
# METIS execution
###############################################################################

def runGraphchk(metis, graphfile):
    # We called this function because we're being verbose, no need to check
    print("-> Verifying graph file '{}' <-".format(graphfile))

    try:
        args = [ metis + "/graphchk", graphfile ]
        out = subprocess.check_output(args, stderr=subprocess.STDOUT)
    except Exception as e:
        print("ERROR: generated a bad graph file - {}".format(e))
        sys.exit(1)

def runPartitioner(metis, graphfile, nodes, suffix, verbose):
    ''' Run the gpmetis program to partition a graph. '''
    global prefix

    if verbose: print("-> Placing threads across {} nodes <-".format(nodes))

    try:
        args = [ metis + "/gpmetis", graphfile, str(nodes), '-niter=100',
                 '-ncuts=100' ]
        out = subprocess.check_output(args, stderr=subprocess.STDOUT)
    except Exception as e:
        print("ERROR: could not run gpmetis - {}".format(e))
        sys.exit(1)

    metisOutput = prefix + suffix + ".metis.out"
    with open(metisOutput, 'w') as mfp: mfp.write(out.decode("utf-8"))
    partitioning = graphfile + ".part.{}".format(nodes)

    assert os.path.isfile(partitioning), \
        "No partitioning file '{}'".format(partitioning)

    return metisOutput, partitioning

###############################################################################
# Driver
###############################################################################

def placeThreads(graph, region, nodes, tidmap, metis,
                 schedule, save, verbose):
    ''' Given a thread/page access graph, place threads across nodes to
        minimize cross-node page accesses.
    '''
    if verbose: print("-> Generating schedule for region {} <-".format(region))

    # Avoid collisions!
    suffix = str(region)
    while os.path.isdir("place-threads-" + suffix):
        suffix = str(random.randint(0, 65536))

    ptids = parseTIDMapFile(tidmap, verbose)
    graphfile, indexes = writeGraphToFile(graph, ptids, suffix, verbose)
    if verbose: runGraphchk(metis, graphfile)
    metisOut, partitioning = runPartitioner(metis, graphfile, nodes, suffix,
                                            verbose)
    writeThreadPlacements(schedule, region, indexes, partitioning, verbose)

    if save:
        dirname = "place-threads-" + suffix + "/"
        if verbose: print("-> Saving partitioning to '{}' <-".format(dirname))
        os.mkdir(dirname)
        writeReadme(region, graph, metis, nodes, dirname, suffix)
        if graph.supportsAdjacencyPrinting(): writeGraphAsMatrix(dirname, graph)
        os.rename(graphfile, dirname + path.basename(graphfile))
        os.rename(metisOut, dirname + path.basename(metisOut))
        os.rename(partitioning, dirname + path.basename(partitioning))
    else:
        os.remove(graphfile)
        os.remove(metisOut)
        os.remove(partitioning)

