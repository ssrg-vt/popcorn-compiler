#!/usr/bin/python

import time
import subprocess
import sys, os
import errno
import signal
import shutil
import xmlrpclib

DEBUG= False#True

### Path Configurations
SCHEDULER_FOLDER=os.getcwd()
HERMIT_INSTALL_FOLDER="%s/hermit-popcorn/" % os.path.expanduser("~")
PROXY_BIN_X86=os.path.join(HERMIT_INSTALL_FOLDER,"x86_64-host/bin/proxy")
PROXY_BIN_ARM=os.path.join(HERMIT_INSTALL_FOLDER,"aarch64-hermit/bin/proxy")
BIN_FOLDER=os.path.join(SCHEDULER_FOLDER,"bins")

def get_env(var, default):
    try:
        return os.environ[var]
    except:
        return default

### Where to run the experiments
EXPERIMENTS_DIR=get_env("HERMIT_EXPERIMENTS_DIR", "/tmp/hermit-scheduler/")

### Machine Configurations
BOARD_NAME=get_env("HERMIT_BOARD_NAME", "potato")
BOARD_NB_CORES=int(get_env("HERMIT_BOARD_NB_CORE", 4))
SERVER_NB_CORES=int(get_env("HERMIT_SERVER_NB_CORE", 4))
BOARD_MEMORY=int(get_env("HERMIT_BOARD_MEMORY", (2*(2**30)))) # 2GB



### Global variables
#Time of the experiment
timer=int(sys.argv[2])
#List of applications. Example: "ep ep". All application must be in BIN_FOLDER
app_list=sys.argv[1].split()
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
    def _start_hw_counter(self):
        path = os.path.join(self.dst_dir, self.HW_COUNTER_FILE)
        self.hw_counter_file_path = path
        f = open(path, "w")
        HW_COUNTER_PERIOD=1000 #in ms
        log("starting perf on", self.dst_dir, self.pid)
        self.monitor=subprocess.Popen(["perf", "stat", "-x|", "-e cache-references", "-I"+str(HW_COUNTER_PERIOD), "-p", str(self.pid)], stdout=f, stderr=f)
    def _stop_hw_counter(self):
        self.monitor.kill()
    def _start_memory_usage(self):
        self.memory_usage_file_path = path = os.path.join(self.dst_dir,self.MEMORY_USAGE_FILE)
    def _start_sched_status(self):
        self.sched_status_file_path = path = os.path.join(self.dst_dir,self.SCHED_STATUS_FILE)

    def _get_dir_usage(self):
        total_size = 0
        for dirpath, dirnames, filenames in os.walk(self.dst_dir):
            for f in filenames:
                fp = os.path.join(dirpath, f)
                total_size += os.path.getsize(fp)
        return total_size

    def _get_last_line(self, fn, average_line=1024):
        log("file name is", fn)
        last=None
        try:
            fh=open(fn, 'r')
            lines=fh.readlines()
            if len(lines)> 0:
                last = lines[-1]
        except:
            pass
        return last

    def _get_int(self, elem):
        try:
            return int(elem)
        except:
            return -1
    def _get_hw_counter_usage(self):
        print("hwf", self.hw_counter_file_path)
        line=self._get_last_line(self.hw_counter_file_path)
        if not line:
            return -1;
        log("perf line is", line)
        elms=line.split('|')
        return self._get_int(elms[1])
    def _get_memory_usage(self):
        line=self._get_last_line(self.memory_usage_file_path)
        if not line:
            return -1;
        return self._get_int(line)
    def _get_sched_status(self):
        line=_get_last_line(self.sched_status_file_path)
        if not line:
            return -1;
        elms=line.split(':')
        return self._get_int(elms[1])
    def get_migration_score(self):
        "Return a migration positive integer score (less is better) or -1 (cannot (yet) be migrated) "
        processor_usage=self._get_hw_counter_usage()
        memory_usage=self._get_memory_usage()
        log("perf results", processor_usage);
        log("memory usage", memory_usage);
        if processor_usage==-1 or memory_usage==-1 or  \
                memory_usage>BOARD_MEMORY/BOARD_NB_CORES:
            return -1
        return processor_usage*memory_usage
    def stop(self):
        self._stop_hw_counter()
    def migrated(self, proc):
        self._update_proc(proc)
        self.stop()
    def __str__(self):
        attrs = vars(self)
        return ', '.join("%s: %s" % item for item in attrs.items())
    def __repr__(self):
        return self.__str__()

def log_action(action, ddir):
    log(action+" application", ddir)
def log_migration(pid):
    global migrated
    migrated+=1
    log_action("Migrated", migrated_app[pid].dst_dir)

def __terminated(pid, lst, tp):
    ddir=lst[pid].dst_dir
    log_action("Terminated ("+tp+")", ddir)
    lst[pid].stop()
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
    os.kill(pid, signal.SIGUSR1)#TODO: catch exception

    ### Wait for checkpoituing to finish
    #w/o on-demande: wait for uhyve to finish
    rpid, status=os.waitpid(pid, 0)
    log(rpid, "checkpointing done with status", status, "return code", running_app[pid].proc.returncode)
    log(rpid, "WIFEXITED", os.WIFEXITED(status), "exit code", os.WEXITSTATUS(status))
    log(rpid, "WIFSIGNALED", os.WIFSIGNALED(status))
    log(rpid, "WIFSTOPPED", os.WIFSTOPPED(status))
    
    ###Send files: copy the whole repository
    dst_dir=running_app[pid].dst_dir
    subprocess.call(["ssh", BOARD_NAME, "mkdir -p", EXPERIMENTS_DIR], stdout=f, stderr=f)
    subprocess.call(["rsync", "-r", dst_dir, BOARD_NAME+":"+EXPERIMENTS_DIR], stdout=f, stderr=f)
    #subprocess.call(["rsync", "--no-whole-file", PROXY_BIN_ARM, BOARD_NAME+":~/"])

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
    rapp.migrated(proc)
    migrated_app[proc.pid]=rapp
    log_migration(proc.pid)
    del running_app[pid]

    return 

def migrate_sort(kv):
    pid, rapp=kv
    score=rapp.get_migration_score()
    if score == -1:
        score=sys.maxint
    return score

def migrate(running_app):
    #if no core on remote machine
    if BOARD_NB_CORES <= len(migrated_app):
        return False
    #check-for/get an arm affine application
    best, bproc=sorted(running_app.items(), key=migrate_sort)[0]
    log("Best application to migrate is", bproc)
    if best==sys.maxint:
        log("Cannot migrate any application", running_app)
        return False
    else:
        __migrate(best)
        return True
            
def _procs_wait(apps):
    for pid, rapp in apps.items():
        ret=rapp.proc.poll()
	    log("proc_wait, pid", pid, "ret code", ret)
        if ret!= None:
            return pid, ret
    return None
def procs_wait(quantum=1):
    ret=_procs_wait(running_app)
    if ret != None:
        return ret
    ret=_procs_wait(migrated_app)
    if ret!=None:
        return ret
    time.sleep(quantum)
    return None, None

def scheduler_wait():
    #if there still a core on Xeon: continue
    log("number of running apps:", len(running_app))
    if len(running_app) < SERVER_NB_CORES:
        return
    #else try to free a core by migrating
    if migrate(running_app):
        return
    #else: just wait for a core to get freed
    pid, ret_code=procs_wait() 
    if pid == None:
        log("Quantum finished with no process finished, pid", pid);
        return
    log("Appplication", pid, "finished with ret code", ret_code);


    if pid in running_app:
        local_terminated(pid)
    elif pid in migrated_app:
        remote_terminated(pid)
    else:
        log("Process", pid, "terminated!")

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
    proc=subprocess.Popen([PROXY_BIN_X86, "prog_x86-64_aligned"], env=env, stdout=f, stderr=f) #TODO:check error

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
                proc.kill() #just kill it
    __cleanup(running_app, local_terminated)
    __cleanup(migrated_app, remote_terminated)

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
#   - check that PROXY_BIN_ARM, BIN_FOLDER exist
try:
    os.makedirs(EXPERIMENTS_DIR)
except OSError as e:
    if e.errno != errno.EEXIST:
        raise
log("NB_CORE_BOARD",BOARD_NB_CORES)
log("NB_CORE_SERVER",SERVER_NB_CORES)

main()

# vim: tabstop=4 expandtab shiftwidth=4 softtabstop=4
