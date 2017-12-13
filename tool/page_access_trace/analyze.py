#!/usr/bin/python3

'''
Analyze a page access trace (PAT) file.
'''

import sys
import argparse
import pat
import metisgraph

###############################################################################
# Parsing
###############################################################################

def parseArguments():
    parser = argparse.ArgumentParser(
        description="Analyze page access trace (PAT) files.",
        formatter_class=argparse.ArgumentDefaultsHelpFormatter)

    config = parser.add_argument_group("Configuration Options")
    config.add_argument("-i", "--input", type=str, required=True,
                        help="Input page access trace file")
    config.add_argument("-o", "--output", type=str, default="out.graph",
                        help="Output graph partitioning file")
    config.add_argument("-p", "--gpmetis", type=str, default="gpmetis",
        help="Location of the 'gpmetis' graph partitioning executable")
    config.add_argument("-v", "--verbose", type=bool, default=True,
                        help="Output verbose graph files")

    # TODO make partitioning selectable from command line
    # TODO get binaries, add option to dump which symbols cause most page faults

    return parser.parse_args()

###############################################################################
# Driver
###############################################################################

if __name__ == "__main__":
    args = parseArguments()
    graph = pat.parsePATtoGraph(args.input)
    metisgraph.writeGraphToFile(graph, args.output, args.verbose)

