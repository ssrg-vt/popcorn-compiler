#!/usr/bin/python3

import sys
import copy
import enum

def percent(numerator, denominator):
    if denominator <= 0: return sys.float_info.max
    else: return (numerator / denominator) * 100

class Configuration:
    def __init__(self, capThresh, startThresh, retThresh, funcThresh):
        self.capThresh = capThresh
        self.startThresh = startThresh
        self.retThresh = retThresh
        self.funcThresh = funcThresh

    def __str__(self):
        def funcThreshStr(funcThresh):
            ret = ""
            for func in funcThresh:
                ret += "{}={},".format(func, funcThresh[func])
            return ret[:-1]

        return "capacity={}, start={}, return={}, functions={}".format(
            self.capThresh, self.startThresh, self.retThresh, \
            funcThreshStr(self.funcThresh))

    def reduceCapThresh(self):
        self.capThresh /= 2
        if self.capThresh == 0:
            self.capThresh = 1
            return False
        else: return True

    def reduceStartThresh(self):
        self.startThresh /= 2
        if self.startThresh == 0:
            self.startThresh = 1
            return False
        else: return True

    def reduceRetThresh(self):
        self.retThresh /= 2
        if self.retThresh == 0:
            self.retThresh = 1
            return False
        else: return True

    def reduceFuncCap(self, function):
        assert function in self.funcThresh,
               "Invalid function, not contained in per-function thresholds"
        self.funcThresh[function] /= 2
        if self.funcThresh[function] == 0:
            self.funcThresh[function] = 1
            return False
        else: return True

class Action(Enum.Enum):
    ReduceCapThresh = 0
    ReduceStartThresh = 1
    ReduceRetThresh = 2
    ReduceFuncCapThresh = 3

class ConfigureHTM:
    def __init__(self, arch, stopThresh, targetTime, resultsFolder):
        assert stopThresh > 0, "Invalid stopping percentage"
        assert targetTime > 0, "Invalid uninstrumented runtime"
        self.keepGoing = True
        self.arch = arch
        self.configurations = [ Configuration(95, 95, 95, {}) ]
        self.runtimes = []
        self.actions = []
        self.stopPercent = stopPercent + 1.0
        self.targetTime = targetTime
        self.decisions = open(resultsFolder + "/decision-log.txt", 'w')

    def __del__(self):
        self.decisions.close()

    def getConfiguration(self):
        curConfig = self.configurations[-1]
        return curConfig.capThresh, curConfig.startThresh, \
               curConfig.retThresh, curConfig.funcThresh

    def analyze(self, runtime, eventCounters, symbolSamples):
        self.runtime.append(runtime)
        newConfig = copy.deepcopy(self.configurations[-1])
        self.configurations.append(newConfig)

        # TODO Rob pick up here!
        # TODO analyze if we made progress towards the goal

        if highAbortRate(self,arch, eventCounters, 0.9):
            # See if more than one function has a high abort rate, or if a
            # single function has a higher rate.
            idx = funcHasHighSamples(symbolSamples, 0.15)
            if idx == 1:
                function = symbolSamples[0][0]
                if function in newConfig.funcThresh:
                    if not newConfig.reduceFuncCap(function):
                        self.keepGoing = False
            else if idx > 0:
                if not newConfig.reduceCapThresh(): self.keepGoing = False
            else:
                self.keepGoing = False
        else:
            # TODO logic for over-instrumented applications
            self.keepGoing = False

