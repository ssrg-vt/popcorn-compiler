#!/usr/bin/env python
# vim: expandtab ts=4 sts=4 sw=4
# Convert page access/fault traces from the kernel ftrace to the page access analysis tool input.
# FYI, collect the trace from '/sys/kernel/debug/tracing/trace_pipe'.

import sys
start = 0.0;

if len(sys.argv) < 2:
    print "usage: %s <ftrace output>" % sys.argv[0]
    sys.exit(1);

with open(sys.argv[1]) as f:
    for l in f.xreadlines():
        e = l[33:].strip().split();
        if len(e) < 2: continue;

        fn = e[1];
        if fn == "handle_mm_fault:":
            # 4068.553120: handle_mm_fault: 1735 R 447bb5 447bb5 0
            #addr = int(e[5], 16) & ~(0xfff);
            nid = "0"
            pid = e[2]
            rw = e[3]
            instr_addr = e[4]
            addr = e[5]
            region = e[6]
        elif fn == "page_server_handle_pte_fault:":
            # 837.232086: page_server_handle_pte_fault: 0 1644 W 406420 768000 1 4096
            if e[8] == "1024" or e[8] == "4096": continue;
            nid = e[2];
            pid = e[3];
            rw = e[4];
            instr_addr = e[5];
            addr = e[6];
            region = e[7];
        elif fn == "process_remote_page_request:":
            # 837.295996: process_remote_page_request: 3 1655 W 4179bd 7ffff1e5a000 0 0
            if e[8] == "1024": continue;
            nid = e[2];
            pid = e[3];
            rw = e[4];
            instr_addr = e[5];
            addr = e[6];
            region = e[7];
        elif fn == "__claim_local_page:" or fn == "__claim_remote_page:":
            nid = 0;
            pid = e[2];
            rw = "I";
            inst_addr = e[3];
            addr = e[4];
            region = e[5];    # peers bitmap
        else:
            continue

        t = float(e[0][:-1])
        if start == 0.0: start = t;

        print "%f %s %s %s %s %s %s" % (t - start, nid, pid, rw, instr_addr, addr, region);
