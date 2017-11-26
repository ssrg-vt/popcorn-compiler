#!/usr/bin/python3

import os
import sys
import time
import random
import argparse
import subprocess

###############################################################################
# Helpers
###############################################################################

def parseArguments():
    desc = "Run an application, while periodically pinging it with the " \
           "migration signal"

    parser = argparse.ArgumentParser(description=desc,
        formatter_class=argparse.ArgumentDefaultsHelpFormatter)

    config = parser.add_argument_group("Configuration")
    config.add_argument("-run-cmd", type=str, required=True,
        help="Command to run the binary",
        dest="runCmd")
    config.add_argument("-signal", type=int, default=35,
        help="Migration signal number",
        dest="signal")
    config.add_argument("-period", type=float, default=0.5,
        help="Period with which to ping the application to migrate",
        dest="period")
    config.add_argument("-output", type=str, default="migration-response.txt",
        help="Output from process",
        dest="out")
    config.add_argument("-verbose", action="store_true",
        help="Verbose printing",
        dest="verbose")

    return parser.parse_args()

def runProcess(args):
    # Assert binary is available & start the binary
    cmd = args.runCmd.split()
    cmd[0] = os.path.abspath(cmd[0])
    assert os.path.isfile(cmd[0]), "Invalid binary '{}'".format(cmd[0])
    try:
        if args.verbose:
            print("Running process '{}'...".format(cmd[0]))

        process = subprocess.Popen(cmd,
                                   stdout=subprocess.PIPE,
                                   stderr=subprocess.PIPE)
    except Exception as e:
        print("Could not run the binary ({})".format(e))
        sys.exit(1)
    return process

def signalProcess(args, process):
    # Generate a little bit of randomness in how often we signal
    delta = args.period * 0.1
    low = args.period - delta
    high = args.period + delta
    numSignalled = 0

    if args.verbose:
        print("Signalling every {} seconds, += {}".format(args.period, delta))

    # Periodically (+/- delta) signal the application
    time.sleep(random.uniform(low, high))
    while process.poll() is None:
        process.send_signal(args.signal)
        time.sleep(random.uniform(low, high))
        numSignalled += 1

    if args.verbose: print("Signalled {} times".format(numSignalled))

def writeOutput(args, process):
    assert process.poll() is not None, "Process didn't finish"
    with open(args.out, 'w') as fp:
        if args.verbose: print("Writing output to '{}'".format(args.out))
        out, err = process.communicate()
        assert out is not None, "No output from process"
        fp.write(out.decode("utf-8"))

###############################################################################
# Driver
###############################################################################

if __name__ == "__main__":
    args = parseArguments()
    process = runProcess(args)
    signalProcess(args, process)
    writeOutput(args, process)

