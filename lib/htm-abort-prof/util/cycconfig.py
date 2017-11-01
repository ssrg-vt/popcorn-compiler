#!/usr/bin/python3

import sys
import copy
import itertools
import perfscrape

def percent(numerator, denominator):
    if denominator <= 0: return sys.float_info.max
    else: return (float(numerator) / float(denominator)) * 100.0

'''
The main driver class which analyzes results and makes decisions.

TODO this needs to be unified with ConfigureHTM.
'''
class ConfigureCycles:
    def __init__(self, targetTime, slowdownThresh, maxIters, resultsFolder):
        self.keepGoing = True
        self.iteration = 1
        self.maxIters = maxIters
        self.targetTime = targetTime
        self.stopRuntime = targetTime * ((float(slowdownThresh) + 100) / 100.0)
        self.decisions = open(resultsFolder + "/decision-log.txt", 'w', 1)
        self.results = []

        # TODO these parameters need to be fine-tuned
        self.cap = 95
        self.start = 95
        self.ret = 95
        self.cycVals = [1, 10, 25, 50, 100, 250, 500, 1000, 2500, 5000]
        self.itersPerMigPoint = [2048, 8192, 32768, 1048576, 4294967295]
        self.configs = list(itertools.product(self.cycVals,
                                              self.itersPerMigPoint))

    def __del__(self):
        self.decisions.close()

    def log(self, msg):
        self.decisions.write("[ Iteration {:>2} ] {}\n" \
                             .format(self.iteration, str(msg)))

    def logFinal(self, msg):
        self.decisions.write("[ Final Result ] {}\n".format(str(msg)))

    def getConfiguration(self):
        curConfig = self.configs[self.iteration - 1]
        self.log("Configuration: capacity={}, start={}, return={}, " \
                 "cycles={}, max iters per migration point={}" \
                 .format(self.cap, self.start, self.ret, curConfig[0],
                         curConfig[1]))
        cyclesArg = "-mllvm -migpoint-cycles={} " \
                    "-mllvm -max-iters-per-migpoint={}" \
                    .format(curConfig[0], curConfig[1])
        return self.cap, self.start, self.ret, cyclesArg

    def analyze(self, time, counters, numSamples, symbolSamples, respStats):
        # TODO need to use htmconfig's Result class
        self.results.append(time)
        slowdown = percent(time, self.targetTime) - 100.0

        self.log("Results from configuration: {:.3f}s ({:.2f}% slowdown)" \
                 .format(time, slowdown))

        def statStr(stats):
            result = ""
            for stat in sorted(stats.keys()):
                result += "{}={:.1f}, ".format(stat, stats[stat])
            return result[:-2]
        self.log("Response time statistics: {}".format(statStr(respStats)))

        if self.iteration > self.maxIters:
            self.log("Hit maximum number of iterations")
            self.keepGoing = False
            return

        if self.iteration >= len(self.cycVals):
            self.log("No more cycle targets to test")
            self.keepGoing = False
            return

        self.iteration += 1

    '''
    Write the best result to the file.  The best result is the configuration
    which had the lowest runtime while still covering the minimum amount of the
    application in transactional execution.
    '''
    def writeBest(self):
        pairs = zip(self.cycVals, self.results)
        best = None
        for pair in pairs:
            if best == None: best = pair
            elif pair[1] < best[1]: best = pair
        slowdown = percent(best[1], self.targetTime) - 100.0
        self.logFinal("Best configuration:")
        self.logFinal("Time: {:.3f}s, {:.2f}% slowdown, {} million cycles" \
                      .format(best[1], slowdown, best[0]))

