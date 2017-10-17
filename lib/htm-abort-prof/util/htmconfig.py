#!/usr/bin/python3

import sys
import copy

def percent(numerator, denominator):
    if denominator <= 0: return sys.float_info.max
    else: return (float(numerator) / float(denominator)) * 100.0

'''
Reduce a threshold's value.
'''
def reduceThresh(val):
    assert val > 0 and val <= 100, "Invalid threshold value"
    assert False, "Need to pick up here"

'''
All perf results for a given configuration.  Includes both HTM event counters
and abort location sampling.
'''
class Result:
    def __init__(self, time, counters, numSamples, symbolSamples):
        self.time = time
        self.counters = counters
        self.numSamples = numSamples
        self.symbolSamples = symbolSamples

    '''
    Return the percentage of cycles of the application that were under
    transactional execution.
    '''
    def percentTransactional(self):
        transactCycles = getTransactCycles(self.counters)
        cycles = getCycles(self.counters)
        assert transactCycles != 0 and cycles != 0, "Invalid counter values"
        return percent(transactCycles, cycles)

    '''
    Return the number of functions whose sample rates for abort events is
    higher than a given threshold.

    Return values:
        HighAbortFuncs: list<str>, list of functions with abort rates above the
                                   threshold
    '''
    def getHighAbortFuncs(self, percent):
        # perf-report should have sorted the list in descending order of sample
        # rate, iterate until we find a value below the threshold.
        SymPercentList = getHTMAbortLocs(self.symbolSamples)
        HighAbortFuncs = []
        for Pair in SymPercentList:
            if Pair[1] > percent: HighAbortFuncs.append(Pair[0])
            else: break
        return HighAbortFuncs

    '''
    Getters for various result values.
    '''

    def getTime(self):
        return self.time

    def getNumSamples(self):
        return self.samples

    def getCounter(self, key):
        assert key in self.counters, \
               "Event '{}' not in counter data".format(key)
        return self.counters[key]

    def getSymbolSample(self, key):
        assert key in self.symbolSamples, \
               "Symbol '{}' not in sampling data".format(key)
        return self.symbolSamples[key]

'''
A configuration for a given piece of the application.
'''
class Configuration:
    def __init__(self, name, cap = 95, start = 95, ret = 95):
        self.name = name
        self.cap = cap
        self.start = start
        self.ret = ret

    def __str__(self):
        return "{}: capacity={}, start={}, return={}".format(
            self.name, self.cap, self.start, self.ret)

    def copy(self, rhs):
        self.cap = rhs.cap
        self.start = rhs.start
        self.ret = rhs.ret

'''
Function-specific configuration.  Maintains the number of iterations we've
tuned a particular function to avoid pathologically exploring a function's
configuration.
'''
class FunctionConfiguration(Configuration):
    def __init__(self, name, cap = 95, start = 95, ret = 95):
        Configuration.__init__(self, name, cap, start, ret)
        self.iter = 0

    def __str__(self):
        return "{}, # tunings={}".format(Configuration, self.iter)

class ConfigureHTM:
    def __init__(self, targetTime, slowdownThresh, maxIters,
                 maxFuncIters, resultsFolder):
        self.keepGoing = True
        self.iter = 0
        self.maxIters = maxIters
        self.maxFuncIters = maxFuncIters
        self.stopRuntime = targetTime * ((float(slowdownThresh) + 100) / 100.0)
        self.decisions = open(resultsFolder + "/decision-log.txt", 'w')

        # There's a 1-to-1 correspondence between each of the following, i.e.,
        # globalConfig[3] & functionConfig[3] generated results[3]
        self.globalConfig = [ Configuration("Global") ]
        self.functionConfig = [ {} ]
        self.results = []

        # TODO these parameters need to be fine-tuned
        # Minimum percent of execution that should be covered in transactions
        self.minCovered = 80.0

        # Percent at which a function is considered to have a high abort rate
        self.highAbort = 5.0

    def __del__(self):
        self.decisions.close()

    def log(self, msg):
        self.decisions.write("[Iteration {}] {}\n".format(self.iter, str(msg)))

    def getConfiguration(self):
        # buildBinary() expects a function -> capacity threshold dictionary
        configDict = {}
        functionConfig = self.functionConfig[-1]
        for func in functionConfig:
            configDict[func] = functionConfig[func].cap

        globalConfig = self.globalConfig[-1]
        return globalConfig.cap, globalConfig.start, \
               globalConfig.ret, configDict

    def analyze(self, time, counters, numSamples, symbolSamples):
        self.results.append(Result(time, counters, numSamples, symbolSamples))
        CurResult = self.results[-1]
        percentTransactional = CurResult.percentTransactional()
        self.iter += 1

        log("Results from configuration: {}s, {%.2f}% covered".format( \
            time, percentTransactional))

        # If we've exhausted our max, we can't do any further configuration.
        if self.iter >= self.maxIters:
            log("Hit maximum number of iterations")
            self.keepGoing = False
            return

        # Set up a new configuration which will be modified according to the
        # performance results from the previous configuration.
        # TODO we should analyze whether we did "better" or roll back if not
        newGlobalConfig = copy.deepcopy(self.globalConfig[-1])
        newFuncConfig = copy.deepcopy(self.functionConfig[-1])
        self.globalConfig.append(newGlobalConfig)
        self.functionConfig.append(newFuncConfig)

        if percentTransactional < self.minCovered:
            log("High abort rate, reducing HTM granularity")

            HighAbortFuncs = CurResult.getHighAbortFuncs(self.highAbort)
            if len(HighAbortFuncs) > 5:
                # A bunch of functions have high abort rates, cut down the
                # overall capacity threshold.
                newGlobalConfig.cap = reduceThresh(newGlobalConfig.cap)
                log("{} have high abort rates, reducing overall " \
                    "capacity threshold to {}" \
                    .format(HighAbortFuncs, newGlobalConfig.cap))
            else:
                # Reduce capacity threshold for the function with the highest
                # abort rates and which has not been previously analyzed too
                # many times
                FuncConfig = None
                for Func in HighAbortFuncs:
                    if Func not in newFuncConfig:
                        FuncConfig = FunctionConfiguration(Func)
                        FuncConfig.copy(newGlobalConfig)
                        newFuncConfig[Func] = FuncConfig
                        break
                    elif newFuncConfig[Func].iter < self.maxFuncIters:
                        FuncConfig = newFuncConfig[Func]
                        break

                if FuncConfig == None:
                    log("All candidates have been fully evaluated!")
                    self.keepGoing = False
                    return

                FuncConfig.iter += 1
                FuncConfig.cap = reduceThresh(FuncConfig.cap)
                log("Reducing capacity threshold for '{}' to {}".format(
                    FuncConfig.name, FuncConfig.cap))

        elif CurResult.time > self.stopRuntime:
            # TODO need to implement reduced instrumentation
            log("Over-instrumented application")
            self.keepGoing = False
        else:
            # We've met both criteria -- we're done!
            log("Hit abort rate & overhead targets!")
            self.keepGoing = False

    '''
    Write the best result to the file.  The best result is the configuration
    which had the lowest runtime while still covering the minimum amount of the
    application in transactional execution.
    '''
    def writeBest():
        BestResult = None
        BestTime = sys.float_info.max
        for Result in self.results:
            if Result.percentTransactional() >= self.minCovered and \
               Result.time < BestTime:
                BestTime = Result.time
                BestResult = Result

        if BestResult == None:
            log("No configurations covered enough of the application in " \
                "transactional execution!")
            return

        Idx = self.results.index(BestResult)
        GlobalConf = self.globalConfig[Idx]
        FuncConf = self.functionConfig[Idx]
        log("Best configuration:")
        log("Time: {}\nPercent covered: {}".format(
            BestTime, BestResult.percentTransactional()))
        log(GlobalConf)
        for Func in FuncConf:
            log(FuncConf[Func])

