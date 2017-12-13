'''
Implement the API to read & write graph files according to METIS' graph
format.
'''

import sys
import graph

###############################################################################
# Printing
###############################################################################

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

def writeGraphToFile(graph, outfile, verbose):
    with open(outfile, 'w') as out:
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

