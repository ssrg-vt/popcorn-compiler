'''
Implement the API to read & write graph files according to METIS' graph
format.
'''

import os
from os import path
import sys
import graph
import random
import subprocess

prefix = "/tmp/place-threads-"

###############################################################################
# Printing
###############################################################################

def writeReadme(graph, gpmetis, nodes, directory):
    with open(directory + "/README", 'w') as fp:
        fp.write("Partitioning generated from page access trace file '{}'\n" \
                 .format(graph.patFile))
        fp.write("  - Partitioning using '{}'\n".format(gpmetis))
        fp.write("  - Distributing threads across {} nodes\n".format(nodes))

'''
Get the header for the graph file which contains configuration information.
'''
def getHeader(graph):
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
    assert len(vertex.edges) > 0, "No edges for {}".format(str(vertex))
    ret = ""
    for other in vertex.edges:
        ret += "{} {} ".format(indexes[other], vertex.edges[other])
    return ret[:-1]

'''
Write a METIS-formatted graph file.  For a graph G = (V, E) with n = |V|
vertices and m = |E| edges, the file has n+1 lines.  The first line is a header
describing the graph configuration, and the remaining n lines are adjacency
lists for each vertex in the graph.  The file has the following format

n m fmt nvweights
<edge 1> <edge weight 1> <edge 2> <edge weight 2> ...
...

Header:
- The fmt parameter is a 3-digit binary number describing the following
  characteristics (from LSB to MSB):
    - The graph has edge weights
    - The graph has vertex weights
    - The graph has vertex sizes
- The nvweights parameter describes how many vertex weights are associated with
  each vertex in the graph.  If nvweights > 0, then bit 2 of fmt must be set.

Vertics:
- Each line after the header represents an adjacency list for a vertex in the
  graph.  Adjacency lists are maintained in <index, weight> tuples.
- Other vertices are referenced by their location in the graph file, e.g.,
  vertex 1 is on line 1, vertex 40 is on line 40, etc.
- Commented lines begin with '%' and do not affect the indexes of vertices.

See the METIS manual for more information:
http://glaros.dtc.umn.edu/gkhome/fetch/sw/metis/manual.pdf
'''
def writeGraphToFile(graph, suffix, verbose):
    global prefix
    graphfile = prefix + suffix + ".graph"
    if verbose: print("-> Printing METIS graph file '{}' <-".format(graphfile))

    with open(graphfile, 'w') as out:
        out.write(getHeader(graph) + "\n")

        # We reference other vertices by their index in the graph file, so
        # assign an ordering to all TIDs & pages
        vertices = sorted(graph.tids.values()) + sorted(graph.pages.values())
        indexes = {}
        idx = 1
        for vertex in vertices:
            indexes[vertex.name] = idx
            idx += 1

        # Finally, write vertices to file
        for vertex in vertices:
            if verbose:
                out.write(getVertexComment(vertex, indexes[vertex.name]) + "\n")
            out.write(getVertexData(vertex, indexes) + "\n")

    if verbose:
        print("Printed graph with {} vertices and {} edges" \
              .format(graph.getNumVertices(), graph.getNumEdges()))

    # Return indexes for parsing the partitioning file
    return graphfile, indexes

###############################################################################
# METIS execution
###############################################################################

def runPartitioner(gpmetis, graphfile, nodes, suffix, verbose):
    global prefix

    if verbose: print("-> Partitioning into {} nodes".format(nodes))

    try:
        args = [ gpmetis, graphfile, str(nodes) ]
        out = subprocess.check_output(args, stderr=subprocess.STDOUT)
    except Exception:
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

def placeThreads(graph, nodes, gpmetis, save, verbose):
    suffix = str(random.randint(0, 65536))
    graphfile, indexes = writeGraphToFile(graph, suffix, verbose)
    # TODO if verbose, run the graphchk tool
    metisOut, partitioning = runPartitioner(gpmetis, graphfile, nodes, suffix,
                                            verbose)
    # TODO parse & print thread placement

    if save:
        dirname = "place-threads-" + suffix + "/"
        os.mkdir(dirname)
        writeReadme(graph, gpmetis, nodes, dirname)
        os.rename(graphfile, dirname + path.basename(graphfile))
        os.rename(metisOut, dirname + path.basename(metisOut))
        os.rename(partitioning, dirname + path.basename(partitioning))
    else:
        os.remove(graphfile)
        os.remove(metisOut)
        os.remove(partitioning)

