#!/usr/bin/python3

import sys, subprocess

###############################################################################
# Pertinent per-architecture event names
###############################################################################

# Total cycles
Cycles = {
    "x86_64" : "cycles",
    "powerpc64le" : "cycles"
}

# Cycles spent in transactional execution
TransactCycles = {
    "x86_64" : "cycles-t"
}

# Cycles spent in committed transactions
CommittedCycles = {
    "x86_64" : "cycles-ct"
}

# Number of transactions started.
HTMBegins = {
    "x86_64" : "tx-start"
}

# Number of committed transactions.
HTMEnds = {
    "x86_64" : "tx-commit"
}

# Number of aborted transactions.
HTMAborts = {
    "x86_64" : "tx-abort"
}

# Number of transactions aborted due to HTM buffer capacity constraints.
HTMCapacityAborts = {
    "x86_64" : "tx-capacity"
}

# Number of transactions aborted due to memory conflicts.
HTMConflictAborts = {
    "x86_64" : "tx-conflict"
}

###############################################################################
# Parsing functionality
###############################################################################

# Scrape output from perf-stat. Return time elapsed (as reported by perf) and
# a dictionary mapping events to counters.
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

# Scrape output from perf-report. Return the number of samples, the
# (approximate) event count and a list of [symbol, percent] tuples.
def scrapePerfReport(perf, filename):
    args = [perf, "report", "-i", filename, "--stdio"]
    try:
        out = subprocess.check_output(args, stderr=subprocess.STDOUT)
    except Exception:
        print("ERROR: could not run perf-report - {}!".format(e))
        sys.exit(1)

    SamplePercent = []
    SymbolIdx = {}
    skipWarn = False
    for line in out.decode("utf-8").splitlines():
        # Skip perf warning text that may pop up relating to kernel symbols
        if "#" in line: skipWarn = True
        elif not skipWarn: continue

        if "# Samples:" in line: Samples = int(line.strip().split()[2])
        elif "# Event count" in line: Count = int(line.strip().split()[4])
        elif "#" not in line:
            fields = line.split()
            if len(fields) < 5: continue
            if "%" in fields[0]:
                Symbol = Symbol
                if Symbol in SymbolIdx:
                    Idx = SymbolIdx[Symbol]
                    SamplePercent[Idx][1] += float(fields[0][:-1])
                else:
                    SamplePercent.append(Symbol, float(fields[0][:-1]))
                    SymbolIdx[Symbol] = len(SamplePercent) - 1

    assert Samples > 0 and len(SamplePercent) > 0, "No samples!"
    return Samples, Count, SamplePercent

###############################################################################
# Queries on results of instrumentation
###############################################################################

# Using event counters from perf-stat, return whether or not the application
# experienced a high rate of transaction aborts.  This includes raw aborted
# transactions and high numbers of wasted cycles (i.e., transactional cycles
# not committed).  Takes an architecture string, a dictionary mapping events to
# counters (from scrapePerfStat()) and a target threshold, e.g., 0.9 means the
# analysis will return true when either metric is less than 90%.
def highAbortRate(arch, counters, thresh):
    # Return true if HTM commits / HTM begins < thresh (there are a lot of
    # doomed transactions)
    Started = counters[HTMBegins[arch]]
    Committed = counters[HTMEnds[arch]]
    if Started > 0 and (Committed / Started) < thresh: return True

    # Return true if cycles in committed transactions / cycles in transactions
    # < thresh (there are a lot of wasted cycles)
    CommCyc = counters[CommittedCycles[arch]]
    TransCyc = counters[TransactCycles[arch]]
    if TransCyc > 0 and (CommCyc / TransCyc) < thresh: return True

    # Otherwise, we'll consider the rate of aborts acceptable.
    return False

# Using event samples from perf-record, return whether or not one function in
# the application has a significantly higher sample rate versus others.  Takes
# a list of [symbol, percent] tuples (from scrapePerfReport()) and a target
# threshold, e.g., 0.1 means the analysis will return true when sample of a
# given function is 10% higher than the next highest function.  Returns the
# index of the function with the significant "drop-off", or -1 if there is no
# drop off.
def funcHasHighSamples(samplepercent, thresh):
    if len(samplepercent) < 2: return -1
    for i in range(len(samplepercent) - 1):
        High = samplepercent[i][1]
        Next = samplepercent[i+1][1]
        if Next and (High / Next) > (thresh + 1.0): return i + 1
    return -1

