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
    config.add_argument("-period", type=float, default=5.0,
        help="Period with which to ping the application to migrate",
        dest="period")
    config.add_argument("-output", type=str, default="migration-response.txt",
        help="Output from process",
        dest="out")

    return parser.parse_args()

def runProcess(args):
    # Assert binary is available & start the binary
    cmd = args.runCmd.split()
    cmd[0] = os.path.abspath(cmd[0])
    assert os.path.isfile(cmd[0]), "Invalid binary '{}'".format(cmd[0])
    try:
        process = subprocess.Popen(stdout=subprocess.PIPE,
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

    # Periodically (+/- delta) signal the application
    while process.poll() is None:
        process.send_signal(args.signal)
        time.sleep(random.uniform(low, high))

def writeOutput(args, process):
    assert process.poll() is not None, "Process didn't finish"
    with open(args.out, 'w') as fp:
        io = process.communicate()
        fp.write(out.decode("utf-8"))

###############################################################################
# Driver
###############################################################################

if __name__ == "__main__":
    args = parseArguments()
    process = runProcess(args)
    signalProcess(args, process)
    writeOutput

