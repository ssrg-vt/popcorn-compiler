#!/bin/bash

PERF="perf"
PERF_STAT_REPEAT=1
ARCH=$(uname -m)
DO_STAT=1
DO_RECORD=1

###############################################################################
# x86 events
###############################################################################

# Event descriptions gathered from perf output and
# https://download.01.org/perfmon/index/broadwell.html
#
# Event notes:
#
#   cpu/tx-abort/pp: precise locations of TSX aborts
#   cycles: total CPU cycles for application
#   cycles-t: cycles in transactions (committed & aborted)
#   cycles-ct: cycles in committed transactions
#   tx-start: transactions started
#   tx-commit: transactions committed
#   tx-abort: transactions aborted
#   tx-capacity: transactions aborted due to capacity overflow
#   tx-conflict: transactions aborted due to access conflict
X86_RECORD_EVENTS="cycles cpu/tx-abort/pp"
X86_STAT_EVENTS="cycles cycles-t cycles-ct tx-start tx-commit tx-abort
                 tx-capacity tx-conflict"

###############################################################################
# POWER8 events
###############################################################################

# TODO

###############################################################################
# Helper functions
###############################################################################

function print_help {
  echo "Profile transactional execution of an applicaction"
  echo "Usage: tsx_perf.sh [ OPTIONS ] -- <binary> [ BINARY OPTIONS ]"
  echo
  echo "Options:"
  echo "  -ppc64le | -x86_64  : select architecture (& perf events)," \
       "default: $ARCH"
  echo "  -p | -perf bin      : which perf binary to use, default: $PERF"
  echo "  -r | -repeat num    : number of times to repeat for perf stat," \
       "default: $PERF_STAT_REPEAT"
  echo "  -no-stat            : don't run perf-stat"
  echo "  -no-record          : don't run perf-record"
}

function build_cmd {
  local perf="$1 $2"
  local events="$(echo $3 | sed -e 's/\s\+/,/g')"
  echo "$perf -e $events"
}

###############################################################################
# Driver
###############################################################################

while [ "$1" != "--" ] && [ "$1" != "" ]; do
  case $1 in
    -h | -help)
      print_help
      exit 0 ;;
    -ppc64le) ARCH="ppc64le" ;;
    -x86_64) ARCH="x86_64" ;;
    -p | -perf)
      PERF=$2
      shift ;;
    -r | -repeat)
      PERF_STAT_REPEAT=$2
      shift ;;
    -no-stat) DO_STAT=0 ;;
    -no-record) DO_RECORD=0 ;;
    *) echo "WARNING: Unknown argument '$1'" ;;
  esac
  shift
done
shift

if [ "$@" == "" ]; then
  echo "ERROR: please supply a binary & arguments after the '--'"
  exit 1
fi

case $ARCH in
  ppc64le)
    PERF_RECORD_EVENTS="$PPC_RECORD_EVENTS"
    PERF_STAT_EVENTS="$PPC_STAT_EVENTS" ;;
  x86_64)
    PERF_RECORD_EVENTS="$X86_RECORD_EVENTS"
    PERF_STAT_EVENTS="$X86_STAT_EVENTS" ;;
  *)
    echo "ERROR: Unknown architecture '$ARCH'"
    exit 1 ;;
esac

if [ ! -f "$PERF" ]; then
  echo "ERROR: could not find perf '$PERF'"
  exit 1
fi

echo "Running '$@'"

PERF_RECORD_FN="${1}.data"
PERF_STAT_FN="${1}.log"

PERF_RECORD="$(build_cmd "$PERF" "record -o $PERF_RECORD_FN" "$PERF_RECORD_EVENTS") $@"
PERF_STAT="$(build_cmd "$PERF" "stat -o $PERF_STAT_FN -r $PERF_STAT_REPEAT" "$PERF_STAT_EVENTS") $@"

if [ $DO_STAT -eq 1 ]; then
  echo "    -> Collecting counters (repeating $PERF_STAT_REPEAT times) <-"
  $PERF_STAT
fi

if [ $DO_RECORD -eq 1 ]; then
  echo "    -> Collecting abort locations <-"
  $PERF_RECORD
fi

