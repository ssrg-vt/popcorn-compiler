#!/usr/bin/python3

import sys, subprocess, platform

arch = platform.machine()

###############################################################################
# Pertinent per-architecture event names
###############################################################################

# Total cycles
Cycles = {
    "x86_64" : "cycles",
    "powerpc64le" : "cycles"
}

def getCycles(counters):
    global arch
    return counters[Cycles[arch]]

# Cycles spent in transactional execution
TransactCycles = {
    "x86_64" : "cycles-t"
}

def getTransactCycles(counters):
    global arch
    return counters[TransactCycles[arch]]

# Cycles spent in committed transactions
CommittedCycles = {
    "x86_64" : "cycles-ct"
}

def getCommittedCycles(counters):
    global arch
    return counters[CommittedCycles[arch]]

# Number of transactions started.
HTMBegins = {
    "x86_64" : "tx-start"
}

def getHTMBegins(counters):
    global arch
    return counters[HTMBegins[arch]]

# Number of committed transactions.
HTMEnds = {
    "x86_64" : "tx-commit"
}

def getHTMEnds(counters):
    global arch
    return counters[HTMEnds[arch]]

# Number of aborted transactions.
HTMAborts = {
    "x86_64" : "tx-abort"
}

def getHTMAborts(counters):
    global arch
    return counters[HTMAborts[arch]]

# HTM abort locations.
HTMAbortLocs = {
    "x86_64" : "cpu/tx-abort/pp"
}

def getHTMAbortLocs(samples):
    global arch
    return samples[HTMAbortLocs[arch]]

# Number of transactions aborted due to HTM buffer capacity constraints.
HTMCapacityAborts = {
    "x86_64" : "tx-capacity"
}

def getHTMCapacityAborts(counters):
    global arch
    return counters[HTMCapacityAborts[arch]]

# Number of transactions aborted due to memory conflicts.
HTMConflictAborts = {
    "x86_64" : "tx-conflict"
}

def getConflictAborts(counters):
    global arch
    return counters[ConflictAborts[arch]]

###############################################################################
# Parsing functionality
###############################################################################

'''
Scrape output from perf-stat. Return time elapsed (as reported by perf) and a
dictionary mapping events to counters.

Return values:
    Time: float, time to run the benchmark, in seconds
    Counters: dictionary (string->int), mapping of events to counter values
'''
def scrapePerfStat(filename):
    with open(filename, 'r') as fp:
        Counters = {}
        for line in fp:
            if "#" in line or "Performance counter stats" in line:
                continue
            elif "time elapsed" in line:
                Time = float(line.strip().split()[0])
            else:
                fields = line.strip().split()
                if len(fields) < 2: continue
                Counters[fields[1]] = float(fields[0].replace(',', ''))
        assert len(Counters) > 0, "No counter values!"
        return Time, Counters

'''
Scrape output from perf-report. Return the number of samples, the approximate
event count and a list of <symbol, percent> tuples.

Return values:
    NumSamples: dictionary (str->int), mapping of events to number of samples
    EventCount: dictionary (str->int), mapping of events to approximate counter
                                       values
    Symbols: dictionary (str->list<tuple<string, float>>), mapping of events to
                                                           lists of tuples of
                                                           function/percent
                                                           pairs
'''
def scrapePerfReport(perf, filename):
    args = [perf, "report", "-i", filename, "--stdio"]
    try:
        out = subprocess.check_output(args, stderr=subprocess.STDOUT)
    except Exception:
        print("ERROR: could not run perf-report - {}!".format(e))
        sys.exit(1)

    # Dictionaries from event -> number of samples, estimated total number of
    # samples & per-symbol sample percentages, respectively
    NumSamples = {}
    EventCount = {}
    Symbols = {}
    skipWarn = True
    for line in out.decode("utf-8").splitlines():
        # Skip perf warning text that may pop up relating to kernel symbols
        if "#" in line: skipWarn = False
        elif skipWarn: continue

        fields = line.strip().split()
        if "# Samples:" in line:
            Event = fields[5][1:-1]
            if not fields[2][-1].isdigit():
                Samples = int(fields[2][:-1])
                if fields[2][-1] == "K": Samples *= 1000
                elif fields[2][-1] == "M": Samples *= 1000000
                elif fields[2][-1] == "B": Samples *= 1000000000
                else: print("WARNING: unknown sample multiplier '{}'" \
                            .format(fields[2][-1]))
            else: Samples = int(fields[2])
            NumSamples[Event] = Samples
        elif "# Event count" in line: EventCount[Event] = int(fields[4])
        elif "#" not in line:
            if len(fields) < 5 or "%" not in fields[0]: continue

            # Populate a list of <symbol, percent> tuples in output order from
            # perf-report, i.e., descending order
            if Event not in Symbols: Symbols[Event] = []

            Symbol = fields[4]
            Percent = float(fields[0][:-1])
            Symbols[Event].append((Symbol, Percent))

    # Remove events for which we don't have any samples
    for k,v in list(NumSamples.items()):
        if v == 0:
            del NumSamples[k]
            del EventCount[k]
            # perf-report won't print any symbols

    assert len(NumSamples) > 0 and len(EventCount) > 0 and len(Symbols) > 0, \
           "No samples found in perf-record output ({})".format(args)
    assert len(NumSamples) == len(EventCount) and \
           len(NumSamples) == len(Symbols), \
           "Bad parsing -- number of events doesn't match ({})".format(args)

    return NumSamples, EventCount, Symbols

