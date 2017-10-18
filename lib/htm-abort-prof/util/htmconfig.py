#!/usr/bin/python3

import sys
import copy
import perfscrape

def percent(numerator, denominator):
    if denominator <= 0: return sys.float_info.max
    else: return (float(numerator) / float(denominator)) * 100.0

'''
Reduce a threshold's value.  Aggressively search over larger values, but
fine-tune as threshold approaches zero (as results seem to become more
sensitive in this region).
'''
def reduceThresh(val):
    assert val > 0 and val <= 100, "Invalid threshold value"

    if val > 75: val = 75
    elif val > 50: val = 50
    elif val > 25: val = 25
    elif val > 10: val = val - 5
    else: val = val - 2

    return max(val, 1)

def increaseThresh(val):
    assert val > 0 and val <= 100, "Invalid threshold value"
    assert False, "Needs implementin'"

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
        transactCycles = perfscrape.getTransactCycles(self.counters)
        cycles = perfscrape.getCycles(self.counters)
        assert transactCycles != 0 and cycles != 0, "Invalid counter values"
        return percent(transactCycles, cycles)

    '''
    Return the percent of transactions which were aborted due to HTM capacity
    overflows.
    '''
    def capacityAbortRate(self):
        transactions = perfscrape.getHTMBegins(self.counters)
        aborted = perfscrape.getHTMCapacityAborts(self.counters)
        assert transactions > 0, "No transactions"
        return percent(aborted, transactions)

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
        SymPercentList = perfscrape.getHTMAbortLocs(self.symbolSamples)
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

    def getSlowdown(self, target):
        return percent(self.time, target) - 100.0

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
        return "{}: capacity={}%, start={}%, return={}%".format(
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
        self.iteration = 0

    def __str__(self):
        return "{}, # tunings={}".format(Configuration, self.iteration)

class ConfigureHTM:
    def __init__(self, targetTime, slowdownThresh, maxIters,
                 maxFuncIters, resultsFolder):
        self.keepGoing = True
        self.iteration = 1
        self.maxIters = maxIters
        self.maxFuncIters = maxFuncIters
        self.targetTime = targetTime
        self.stopRuntime = targetTime * ((float(slowdownThresh) + 100) / 100.0)
        self.decisions = open(resultsFolder + "/decision-log.txt", 'w')
        self.log("Target stopping time: {:.3f}s".format(self.stopRuntime))

        # There's a 1-to-1 correspondence between each of the following, i.e.,
        # globalConfig[3] & functionConfig[3] generated results[3]
        self.globalConfig = [ Configuration("Global") ]
        self.functionConfig = [ {} ]
        self.results = []
        self.log(self.globalConfig[-1])

        # TODO these parameters need to be fine-tuned
        # Minimum percent of execution that should be covered in transactions
        # and maximum desired abort rate, respectively
        self.minCovered = 90.0
        self.maxAbortRate = 2.0

        # Percent at which a function is considered to have a high abort rate
        self.highAbort = 5.0

    def __del__(self):
        self.decisions.close()

    def log(self, msg):
        self.decisions.write("[ Iteration {:>2} ] {}\n" \
                             .format(self.iteration, str(msg)))

    def logFinal(self, msg):
        self.decisions.write("[ Final Result ] {}\n".format(str(msg)))

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
        slowdown = CurResult.getSlowdown(self.targetTime)
        percentTransactional = CurResult.percentTransactional()
        abortRate = CurResult.capacityAbortRate()

        self.log("Results from configuration: {:.3f}s ({:.2f}% slowdown), " \
                 "{:.2f}% covered, {:.2f}% abort rate" \
                 .format(time, slowdown, percentTransactional, abortRate))

        # If we've exhausted our max, we can't do any further configuration.
        if self.iteration >= self.maxIters:
            self.log("Hit maximum number of iterations")
            self.keepGoing = False
            return

        # Set up a new configuration which will be modified according to the
        # performance results from the previous configuration.
        # TODO we should analyze whether we did "better" or roll back if not
        newGlobalConfig = copy.deepcopy(self.globalConfig[-1])
        newFuncConfig = copy.deepcopy(self.functionConfig[-1])
        self.globalConfig.append(newGlobalConfig)
        self.functionConfig.append(newFuncConfig)

        if percentTransactional < self.minCovered or \
           abortRate > self.maxAbortRate:
            self.log("High abort rate, reducing HTM granularity")

            HighAbortFuncs = CurResult.getHighAbortFuncs(self.highAbort)
            if len(HighAbortFuncs) > 5:
                # A bunch of functions have high abort rates, cut down the
                # overall capacity threshold.
                newCap = reduceThresh(newGlobalConfig.cap)
                if prevCap == newGlobalConfig.cap:
                    self.log("NOTE: functions {} have many aborts, but we " \
                             "can't reduce the global capacity threshold " \
                             "any further".format(HighAbortFuncs))
                    self.keepGoing = False
                    return

                newGlobalConfig.cap = newCap
                self.log("Functions {} have high abort rates, reducing " \
                         "overall capacity threshold to {}" \
                         .format(HighAbortFuncs, newCap))
            else:
                # Reduce capacity threshold for function with highest abort
                # rates and which has not been analyzed too many times
                FuncConfig = None
                for Func in HighAbortFuncs:
                    if Func not in newFuncConfig:
                        FuncConfig = FunctionConfiguration(Func)
                        FuncConfig.copy(newGlobalConfig)
                    elif newFuncConfig[Func].iteration < self.maxFuncIters:
                        FuncConfig = newFuncConfig[Func]
                    else:
                        self.log("NOTE: function '{}' has many aborts but " \
                                 "we ran out of tuning iterations" \
                                 .format(Func))
                        continue

                    # See if we can reduce the capacity threshold further
                    assert FuncConfig != None, "Should have picked a function"
                    newCap = reduceThresh(FuncConfig.cap)
                    if newCap == FuncConfig.cap:
                        self.log("NOTE: function '{}' has many aborts but " \
                                 "we can't reduce its capacity threshold " \
                                 "any further".format(Func))
                        FuncConfig = None
                        continue
                    FuncConfig.cap = newCap
                    break

                if FuncConfig == None:
                    self.log("All candidates have been fully evaluated!")
                    self.keepGoing = False
                    return

                if FuncConfig.name not in newFuncConfig:
                    newFuncConfig[FuncConfig.name] = FuncConfig
                FuncConfig.iter += 1
                self.log("Reducing capacity threshold for '{}' to {}".format(
                    FuncConfig.name, FuncConfig.cap))

        elif CurResult.time > self.stopRuntime:
            # TODO need to implement reduced instrumentation
            self.log("Over-instrumented application!")
            self.keepGoing = False
            return
        else:
            # We've met both criteria -- we're done!
            self.log("Hit abort rate & overhead targets!")
            self.keepGoing = False
            return

        self.iteration += 1

    '''
    Write the best result to the file.  The best result is the configuration
    which had the lowest runtime while still covering the minimum amount of the
    application in transactional execution.
    '''
    def writeBest(self):
        BestResult = None
        BestTime = sys.float_info.max
        for Result in self.results:
            if Result.percentTransactional() >= self.minCovered and \
               Result.time < BestTime:
                BestTime = Result.time
                BestResult = Result

        if BestResult == None:
            self.logFinal("No configurations covered enough of the " \
                          "application in transactional execution!")
            return

        Idx = self.results.index(BestResult)
        GlobalConf = self.globalConfig[Idx]
        FuncConf = self.functionConfig[Idx]
        Slowdown = BestResult.getSlowdown(self.targetTime)
        self.logFinal("Best configuration:")
        self.logFinal("Time: {:.3f}s, {:.2f}% slowdown" \
                      .format(BestTime, Slowdown))
        self.logFinal("Percent covered: {:.2f}%" \
                      .format(BestResult.percentTransactional()))
        self.logFinal
        self.logFinal(GlobalConf)
        for Func in FuncConf:
            self.logFinal(FuncConf[Func])

