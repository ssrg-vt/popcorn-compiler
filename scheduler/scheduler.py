#!/usr/bin/python

import time
import subprocess
import sys, os
import signal
import shutil
import csv
import xmlrpclib

DEBUG=True

### Path Configurations
SCHEDULER_FOLDER=os.getcwd()
HERMIT_INSTALL_FOLDER="%s/hermit-popcorn/" % os.path.expanduser("~")
PROXY_BIN=os.path.join(HERMIT_INSTALL_FOLDER,"x86_64-host/bin/proxy")
BIN_FOLDER=os.path.join(SCHEDULER_FOLDER,"bins")
APP_INFO_FILE=os.path.join(SCHEDULER_FOLDER,"info.csv")

### Where to run the experiments
EXPERIMENTS_DIR=os.environ["HERMIT_EXPERIMENTS_DIR"] or "/tmp/hermit-scheduler/"

### Machine Configurations
BOARD_NAME=os.environ["HERMIT_BOARD_NAME"] or "potato"
BOARD_NB_CORES=int(os.environ["HERMIT_BOARD_NB_CORE"] or 4)
SERVER_NB_CORES=int(os.environ["HERMIT_SERVER_NB_CORE"] or 4)
BOARD_MEMORY=os.environ["HERMIT_BOARD_MEMORY"] or (2*(2**30)) # 2GB

### Global variables
#Time of the experiment
timer=int(sys.argv[2])
#List of applications. Example: "ep ep". All application must be in BIN_FOLDER
app_list=sys.argv[1].split()
#Extracted from the CSV file APP_INFO_FILE
app_info=dict() #load_csv(APP_INFO_FILE)
#The next two args track running and migrated applications (format: key=pid; value=RApp)
running_app=dict()
migrated_app=dict()

### Stat variables
start_time=0
app_count=0
started=0
migrated=0
terminated_local=0
terminated_remote=0

def log(*args):
    if DEBUG:
        print(time.time(),":", args)

class RApp:
    "Represent a running application"
    SCHED_STATUS_FILE=".status"
    MEMORY_USAGE_FILE=".memory"
    HW_COUNTER_FILE=".counter"
    def __init__(self, name, rid, dst_dir, proc):
        self.name=name
        self.rid=rid
        self.dst_dir=dst_dir
        self._update_proc(proc)
        self._start_hw_counter()
        self._start_memory_usage()
        self._start_sched_status()
    def _update_proc(self,proc):
        self.proc=proc
        self.pid=proc.pid
    def _start_hw_counter():
        self.hw_counter_file_path = path = os.path.join(dst_dir,HW_COUNTER_FILE)
        f = open(path, "w")
        HW_COUNTER_PERIOD=1000 #in ms
        subprocess.call(["perf", "stat", "-x|" "-e cache-references", "-I"+str(HW_COUNTER_PERIOD), "-p"+str(self.pid), stdout=f, stderr=f)
    def _start_memory_usage():
        self.memory_usage_file_path = path = os.path.join(dst_dir,MEMORY_USAGE_FILE)
    def _start_sched_status():
        self.sched_status_file_path = path = os.path.join(dst_dir,SCHED_STATUS_FILE)

    def _get_dir_usage():
        total_size = 0
        for dirpath, dirnames, filenames in os.walk(self.dst_dir):
            for f in filenames:
                fp = os.path.join(dirpath, f)
                total_size += os.path.getsize(fp)
        return total_size

    #TODO
    def _get_hw_counter_usage():
        pass#line=get_last_line(self.sched_status_file_path)
    def _get_memory_usage():
        pass #line=get_last_line(self.sched_status_file_path)
    def _get_sched_status():
        pass#line=get_last_line(self.sched_status_file_path)
    def get_migration_score():
        "Return a migration score between 0 (don't migrate) and 100 (migrate)"
        pass

    def migrated(self, proc):
        _update_proc(proc)
    def __str__(self):
        attrs = vars(self)
        return ', '.join("%s: %s" % item for item in attrs.items())
    def __repr__(self):
        return self.__str__()

#Application information (Migrate: 1/0) is in the CSV file
def load_csv(fn):
    res=dict()
    header=None
    csvfile0=open(fn)
    reader = csv.reader(csvfile0, delimiter=',', quotechar='|')
    for row in reader:
        if header == None:
            header=row
            continue
        res[row[0]]=dict()
        for feature in header[1:]:
            res[row[0]][feature]=row[1]
    log(res)
    return res
app_info=load_csv(APP_INFO_FILE)
def is_board_affine(app):
    return int(app_info[app]["Migrate"])

def log_action(action, ddir):
    log(action+" application", ddir)
def log_migration(pid):
    global migrated
    migrated+=1
    log_action("Migrated", migrated_app[pid].dst_dir)

def __terminated(pid, lst, tp):
    ddir=lst[pid].dst_dir
    log_action("Terminated ("+tp+")", ddir)
    del lst[pid]
def local_terminated(pid):
    global terminated_local
    terminated_local+=1
    __terminated(pid, running_app, "local")
def remote_terminated(pid):
    global terminated_remote
    terminated_remote+=1
    __terminated(pid, migrated_app, "remote")

def __migrate(pid):
    f = open("mig.output.txt", "w")

    ### Start checkpoint: send signal
    os.kill(pid, signal.SIGUSR1)

    ### Wait for checkpoituing to finish
    #w/o on-demande: wait for uhyve to finish
    rpid, status=os.waitpid(pid, 0)
    #TODO: check status?
    
    ###Send files: copy the whole repository
    dst_dir=running_app[pid].dst_dir
    subprocess.call(["ssh", BOARD_NAME, "mkdir -p", EXPERIMENTS_DIR], stdout=f, stderr=f)
    subprocess.call(["rsync", "-r", dst_dir, BOARD_NAME+":"+EXPERIMENTS_DIR], stdout=f, stderr=f)
    #subprocess.call(["rsync", "--no-whole-file", PROXY_BIN, BOARD_NAME+":~/"])

    ssh_ags="""HERMIT_ISLE=uhyve HERMIT_MEM=2G HERMIT_CPUS=1    \
        HERMIT_VERBOSE=0 HERMIT_MIGTEST=0                       \
        HERMIT_MIGRATE_RESUME=1 HERMIT_DEBUG=0                  \
        HERMIT_NODE_ID=1 ST_AARCH64_BIN=prog_aarch64_aligned    \
        HERMIT_FULL_CHKPT_RESTORE=1 HERMIT_FULL_CHKPT_SAVE=1
        ST_X86_64_BIN=prog_x86-64_aligned"""+" "
    ssh_ags+="cd "+dst_dir +"; "
    ssh_ags+="~/proxy ./prog_aarch64_aligned "
    #ssh_ags+="echo $!;" #print pid: may be needed if we want to migrate back
    #ssh_ags+="wait" #wait for the process to finishes

    ### Run remotly 
    proc=subprocess.Popen(["ssh", BOARD_NAME, ssh_ags], stdout=f, stderr=f) 
    #proc=subprocess.Popen([RESUME_SCRIPT, dst_dir], stdout=f, stderr=f) 

    ### Remove from running app and add to migrated app
    rapp=running_app[pid]
    rapp.update_proc(proc)
    migrated_app[proc.pid]=rapp
    log_migration(proc.pid)
    del running_app[pid]

    return 


def migrate(running_app):
    #if no core on remote machine
    if BOARD_NB_CORES <= len(migrated_app):
        return False
    #check for an arm affine application
    for pid, rapp in running_app.items(): 
        if is_board_affine(rapp.name):
            __migrate(rapp.pid)
            return True
    return False
            

def scheduler_wait():
    #if there still a core on Xeon: continue
    log("number of running apps:", len(running_app))
    if len(running_app) < SERVER_NB_CORES:
        return
    #else try to free a core by migrating
    if migrate(running_app):
        return
    #else: just wait for a core to get freed
    pid, status=os.wait() 
    #TODO: check status!

    if pid in running_app:
        local_terminated(pid)
    else:
        remote_terminated(pid)

def run_app(app):
    """ Start an application on the server """
    global started

    if len(running_app) >= SERVER_NB_CORES:
        return False

    ### Copy foder of the application to EXPERIMENTS_DIR
    src_dir=os.path.join(BIN_FOLDER, app)
    dst_dir=os.path.join(EXPERIMENTS_DIR, app+str(app_count))
    shutil.copytree(src_dir, dst_dir, symlinks=False, ignore=None)

    ### Start application
    os.chdir(dst_dir)
    env = os.environ
    env["HERMIT_ISLE"]="uhyve"
    env["HERMIT_MEM"]="2G"
    env["HERMIT_CPUS"]="1"
    env["HERMIT_VERBOSE"]="0"
    env["HERMIT_MIGTEST"]="0"
    env["HERMIT_MIGRATE_RESUME"]="0"
    env["HERMIT_DEBUG"]="0"
    env["HERMIT_NODE_ID"]="0"
    env["HERMIT_FULL_CHKPT_SAVE"]="1"
    env["ST_AARCH64_BIN"]="prog_aarch64_aligned"
    env["ST_X86_64_BIN"]="prog_x86-64_aligned"
    f = open("output.txt", "w")
    proc=subprocess.Popen([PROXY_BIN, "prog_x86-64_aligned"], env=env, stdout=f, stderr=f) #TODO:check error

    ### Register application
    running_app[proc.pid]=RApp(app,app_count,dst_dir,proc)
    log("running applications", running_app)
    log_action("Started", dst_dir)
    started+=1

    return True

#TODO: delete folders of applications in /tmp/ (ddir)
def print_report():
    end = time.time()
    print("Started applications", started)
    print("Migrated applications", migrated)
    print("Terminated (local) applications", terminated_local)
    print("Terminated (remote) applications", terminated_remote)
    print("Ending at time", end)
    print("Total time is:", end-start_time)

def cleanup():
    def __cleanup(lst, trm):
        for pid, val in lst.items():
            proc=val.proc
            if proc.poll() is not None:
                trm(pid)
            else:
                proc.kill()
    __cleanup(running_app, local_terminated)
    __cleanup(migrated_app, local_terminated)

def terminate(signum, frame):
    log("Terminating: singnal received is", signum)
    cleanup()
    print_report()
    sys.exit()

def set_timer(timer):
    # Set the signal handler and a "timer" seconds alarm
    signal.signal(signal.SIGALRM, terminate)
    signal.alarm(timer)

############################# Initializaiton functions #############
def main():
    global app_count, start_time
    set_timer(timer)
    start_time = time.time()
    nb_app=len(app_list)
    print("Starting at time", start_time)
    while True:
        scheduler_wait()
        log("Remaining timer:", timer-(time.time()-start_time))
        log("Trying to place an application at:", time.time())
        run_app(app_list[app_count%nb_app])
        app_count+=1


#Handle assumed files/dirs and check them
#TODO:  
#   - check that APP_INFO_FILE exist
#   - check that PROXY_BIN, BIN_FOLDER exist
os.makedirs(EXPERIMENTS_DIR)
log("NB_CORE_BOARD",BOARD_NB_CORES)
log("NB_CORE_SERVER",SERVER_NB_CORES)

main()

# vim: tabstop=4 expandtab shiftwidth=4 softtabstop=4
