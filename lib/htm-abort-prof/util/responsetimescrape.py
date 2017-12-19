#!/usr/bin/python3

import statistics

'''
Scrape response time output from migration library.  Return the arithmetic
mean, median, minimum, maximum, and list of response times.

Return values:
    Stats: dictionary (str->float), contains the following statistics:
           Average: float, arithmetic mean of response times
           Median: float, median of response times
           Min: float, minimum sampled response time
           Max: float, maximum sampled response time
    RespTimes: list<float>, list of response time samples
    NumCalls: int, number of calls into the migration library
'''
def scrapeResponseTimes(filename):
    with open(filename, 'r') as fp:
        RespTimes = []
        Parse = False
        for line in fp:
            if "Response times" in line: Parse = True
            elif not Parse: continue
            elif "migration library" in line: NumCalls = int(line.split()[0])
            else: RespTimes.append(float(line.split()[0]))

    if len(RespTimes) > 0:
        Stats = {
            "average" : statistics.mean(RespTimes),
            "median" : statistics.median(RespTimes),
            "minimum" : min(RespTimes),
            "maximum" : max(RespTimes)
        }
    else:
        Stats = {
            "average" : 0.0,
            "median" : 0.0,
            "minimum" : 0.0,
            "maximum" : 0.0
        }
        NumCalls = 0

    return Stats, RespTimes, NumCalls

