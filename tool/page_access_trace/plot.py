'''
Generate plots to visualize page access patterns.
'''

import matplotlib
import matplotlib.pyplot as plt

def plotPageAccessFrequency(chunks, ranges, perthread, save):
    ''' Plot page fault frequencies over the duration of an application's
        execution.
    '''
    fig = plt.figure(figsize=(12, 4))
    ax = plt.subplot(111)
    if perthread:
        for tid in sorted(chunks.keys()):
            ax.plot(ranges, chunks[tid], label="Thread {}".format(tid))
        box = ax.get_position()
        ax.set_position([box.x0, box.y0, box.width*0.8, box.height])
        legend = ax.legend(loc="center left", shadow=True, fontsize="small",
                           bbox_to_anchor=(1, 0.5))
    else: ax.plot(ranges, chunks)
    plt.xlabel("Seconds")
    plt.ylabel("Number of faults")
    if save == None: plt.show()
    else: plt.savefig(save)

