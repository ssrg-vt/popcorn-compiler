''' Implements an undirected graph G = (V, E), which along with weight & size
    configuration information maintains vertices, edges and their associated
    weights.  Specialized for Popcorn Linux, and in particular the graph where
    vertices represent threads & pages, and edges represent page accesses by
    those threads.
'''

import itertools

class Graph:
    ''' A graph which maintains the raw mappings between TIDs and the pages
        they accessed.  Allows the partitioning tool to specify both a thread
        and page placement.
    '''

    class Vertex:
        ''' A generic vertex in the graph.  An instance maintains the vertex's
            name and edges to other vertexes.  Note that an instance's name
            field should be an integer type.
        '''
        def __init__(self, name):
            # TODO vertex size, vertex weight(s)
            assert type(name) is int, \
                "Invalid type for vertex name, must an integer type"
            self.name = name
            self.edges = {}

        def addEdge(self, vertex, weight):
            ''' Add or update an edge between this vertex and another with
                additional weight.  Return true if the edge being added is new
                in the adjacency list for this vertex, or false if it had been
                previously added.
            '''
            if vertex in self.edges:
                self.edges[vertex] += weight
                return False
            else:
                self.edges[vertex] = weight
                return True

        def __str__(self):
            return str(self.name)

        def __hash__(self):
            return hash(self.name)

        def __eq__(self, other):
            return self.name == other.name

        def __ne__(self, other):
            return self.name != other.name

        def __lt__(self, other):
            return self.name < other.name

    class Thread(Vertex):
        ''' A thread's page accesses.  Maintains a dictionary mapping page
            addresses to the number of times they are accessed by the thread.
        '''
        def __init__(self, tid):
            super().__init__(tid)

        def __getitem__(self, page):
            assert page in self.edges, \
                "Page {} not accessed by {}".format(page, self.__str__())
            return self.edges[page]

        def __str__(self):
            return "thread " + str(self.name)

    class Page(Vertex):
        ''' A page accessed by threads.  Maintains a dictionary mapping thread
            IDs to the number of times they access the page.
        '''
        def __init__(self, page):
            super().__init__(page)

        def __getitem__(self, tid):
            assert tid in self.edges, \
                "Thread {} did not access {}".format(tid, self.__str__())
            return self.edges[tid]

        def __str__(self):
            return "page @ 0x" + hex(self.name)

    def __init__(self,
                 pageAccessTraceFile,
                 hasEdgeWeights=False,
                 hasVertexSizes=False,
                 numVertexWeights=0):
        assert numVertexWeights >= 0, "Invalid number of vertex weights"
        self.patFile = pageAccessTraceFile
        self.numEdges = 0
        self.hasEdgeWeights = hasEdgeWeights
        self.hasVertexSizes = hasVertexSizes
        self.numVertexWeights = numVertexWeights

        # Keep track of which threads accessed which pages and vice versa
        self.tids = {}
        self.pages = {}

    def getNumVertices(self):
        return len(self.tids) + len(self.pages)

    def getNumEdges(self):
        return self.numEdges

    def addMapping(self, tid, page, weight=1):
        ''' Add an access from a thread to a page. '''
        # TODO is it possible for TIDs to overlap with page addresses?
        if tid not in self.tids: self.tids[tid] = Graph.Thread(tid)
        if page not in self.pages: self.pages[page] = Graph.Page(page)
        if self.tids[tid].addEdge(page, weight): self.numEdges += 1
        self.pages[page].addEdge(tid, weight) # Don't double-count edges

    def postProcess(self):
        ''' Nothing to do, the graph is established by construction in
            Graph.addMapping().
        '''
        return

class InterferenceGraph(Graph):
    ''' A graph that, rather than maintain the raw mappings between TIDS and
        pages, maintains an interference graph directly between threads.
    '''

    def getNumVertices(self):
        return len(self.tids)

    def addMapping(self, tid, page, weight=1):
        ''' Add an access from a thread to a page.  Note that unlike
            Graph.addMapping(), we only maintain access counts on the page
            side.  Thread vertices will be created during post processing.
        '''
        if page not in self.pages: self.pages[page] = Graph.Page(page)
        self.pages[page].addEdge(tid, weight)

    def postProcess(self):
        ''' Calculate interference between threads based on the number of times
            each thread accesses a given page.  Two threads interfere on a page
            if they both accessed it at some point.  The interference is
            quantified as the minimum between the number of times each of the
            two threads accessed the page.  This is an O(n^2) operation, since
            each thread can potentially interfere with each other thread.
        '''
        for page in self.pages:
            curPage = self.pages[page]
            for t in itertools.combinations(curPage.edges.keys(), 2):
                if t[0] not in self.tids: self.tids[t[0]] = Graph.Thread(t[0])
                if t[1] not in self.tids: self.tids[t[1]] = Graph.Thread(t[1])
                weight = min(curPage[t[0]], curPage[t[1]])
                if self.tids[t[0]].addEdge(t[1], weight): self.numEdges += 1
                self.tids[t[1]].addEdge(t[0], weight) # Don't double-count edges

        # Drop pages so we don't add them to the graph file
        # TODO this seems dirty, need better way to disable...
        self.pages = {}

