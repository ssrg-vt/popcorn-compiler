#!/usr/bin/python3

import sys
import numpy as np
import matplotlib
matplotlib.use("Agg")
import matplotlib.pyplot as plt
import responsetimescrape

if __name__ == "__main__":
    assert len(sys.argv) > 1, "Please supply an input file"
    stats, respTimes, numCalls = \
      responsetimescrape.scrapeResponseTimes(sys.argv[1])
    assert len(respTimes) > 0, "File {} doesn't have any response times" \
                               .format(sys.argv[1])

    density, bins = np.histogram(respTimes, bins=200, normed=True, density=True)
    norm_density = density / density.sum()
    plt.bar(bins[1:], norm_density, color="b")
    plt.xlabel("Nanoseconds (ns)")
    plt.ylabel("Probability (%)")
    plt.autoscale()
    plt.grid(True)
    plt.savefig(sys.argv[1] + ".pdf")

