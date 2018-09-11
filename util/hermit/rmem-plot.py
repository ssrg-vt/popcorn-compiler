#!/usr/bin/python

import sys
import matplotlib
import matplotlib.pyplot as plt
import numpy as np

if __name__ == "__main__":
    if len(sys.argv) != 2:
        print("Usage: %s <rmem.log>" % sys.argv[0])

    data = np.genfromtxt(sys.argv[1], delimiter=';')

    timestamps = data[:,0]
    addresses = data[:,1]
    sizeleft = 100 - data[:,4]

    fig, ax1 = plt.subplots()

    plt.grid()
    plt.xlabel("Time after resume (s)")
    plt.ylabel("Virtual address requested", color="blue")
    ax1.tick_params('y', colors="blue")

    ax1.plot(timestamps, addresses, linestyle="", marker="x", color="blue")

    ax2 = ax1.twinx()
    ax2.plot(timestamps, sizeleft, linestyle="", marker="+", color="green")
    ax2.set_ylabel('Percentage of memory served', color='green')
    ax2.tick_params('y', colors='green')

    plt.show()
