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
    served_heap = 100 - data[:,4]
    served_bss = 100 - data[:,6]
    served_data = 100 - data[:,8]

    fig, ax1 = plt.subplots()

    plt.grid()
    plt.xlabel("Time after resume (s)")
    plt.ylabel("Virtual address requested", color="blue")
    ax1.tick_params('y', colors="blue")

    ax1.plot(timestamps, addresses, linestyle="", marker="x", color="blue", label="address")

    ax2 = ax1.twinx()
    ax2.plot(timestamps, served_heap, linestyle="", marker="+", color="lightcoral", label = "% heap served")
    ax2.plot(timestamps, served_bss, linestyle="", marker="+", color="palegreen", label = "% bss served")
    ax2.plot(timestamps, served_data, linestyle="", marker="+", color="lightgray", label = "% data served")
    ax2.set_ylabel('Percentage of memory served')

    ax1.legend(loc=8)
    ax2.legend()
    
    plt.show()
