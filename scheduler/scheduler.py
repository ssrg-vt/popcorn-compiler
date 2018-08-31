#!/usr/bin/python

#send request to start an application to master.py
import time
import subprocess
import sys, os
import signal
import shutil
import csv
import xmlrpclib


#Path Configurations
SCHEDULER_FOLDER=os.getcwd()
HERMIT_INSTALL_FOLDER="%s/hermit-popcorn/" % os.path.expanduser("~")
PROXY_BIN=os.path.join(HERMIT_INSTALL_FOLDER,"x86_64-host/bin/proxy")
BIN_FOLDER=os.path.join(SCHEDULER_FOLDER,"bins")
APP_INFO_FILE=os.path.join(SCHEDULER_FOLDER,"info.csv")
RESUME_SCRIPT=os.path.join(SCHEDULER_FOLDER,"resume.sh")
INSTALL_FOLDER="/tmp/test" #TODO: create real tmp folder

#Machine Configurations
BOARD="potato"
BOARD_NB_CORE=2
SERVER_NB_CORES=2

#Global variables
timer=0
start=0
app_id=0
app_info=[]
app_list=[]

#Stat variables
started=0
migrated=0
terminated_local=0
terminated_remote=0

#format: key=pid; value=RApp
running_app=dict()
migrated_app=dict()

class RApp:
    "represent a running application"
    def __init__(self, name, rid, dst_dir, proc):
        self.name=name
        self.rid=rid
        self.dst_dir=dst_dir
        self.update_proc(proc)
    def update_proc(self,proc):
        self.proc=proc
        self.pid=proc.pid
    def __str__(self):
        attrs = vars(self)
        return ', '.join("%s: %s" % item for item in attrs.items())
    def __repr__(self):
        return self.__str__()

#arguments transforming functions
def load_app_info(file_name):
    af = load_csv(file_name)
    return af
def get_app_list(app_comp):
    return app_comp.split()
def create_taget_dir(dirname):
    os.makedirs(dirname)

#application information (Migrate: 1/0) is in the CSV file
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
    print res
    return res
def is_board_affine(app):
    return int(app_info[app]["Migrate"])

def log_action(action, ddir):
    print(action+" application", ddir)
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
    subprocess.call(["ssh", BOARD, "mkdir -p", INSTALL_FOLDER], stdout=f, stderr=f)
    subprocess.call(["rsync", "-r", dst_dir, BOARD+":"+INSTALL_FOLDER], stdout=f, stderr=f)

    ### Run remotly 
    #FIXME: don't use the script
    proc=subprocess.Popen([RESUME_SCRIPT, dst_dir], stdout=f, stderr=f) 

    ### Remove from running app and add to migrated app
    rapp=running_app[pid]
    rapp.update_proc(proc)
    migrated_app[proc.pid]=rapp
    log_migration(proc.pid)
    del running_app[pid]

    return 


def migrate(running_app):
    #if no core on remote machine
    if BOARD_NB_CORE <= len(migrated_app):
        return False
    #check for an arm affine application
    for pid, rapp in running_app.items(): 
        if is_board_affine(rapp.name):
            __migrate(rapp.pid)
            return True
    return False
            

def scheduler_wait():
    #if there still a core on Xeon: continue
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

    ### Copy foder of the application to INSTALL_FOLDER
    src_dir=os.path.join(BIN_FOLDER, app)
    dst_dir=os.path.join(INSTALL_FOLDER, app+str(app_id))
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
    env["ST_AARCH64_BIN"]="prog_aarch64_aligned"
    env["ST_X86_64_BIN"]="prog_x86-64_aligned"
    f = open("output.txt", "w")
    proc=subprocess.Popen([PROXY_BIN, "prog_x86-64_aligned"], env=env, stdout=f, stderr=f) #TODO:check error

    ### Register application
    running_app[proc.pid]=RApp(app,app_id,dst_dir,proc)
    print("running applications", running_app)
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
    print("Total time is:", end-start)

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
    cleanup()
    print_report()
    sys.exit()

def set_timer(timer):
    # Set the signal handler and a "timer" seconds alarm
    signal.signal(signal.SIGALRM, terminate)
    signal.alarm(timer)

############################# Initializaiton functions #############
def main():
    global app_id, start
    set_timer(timer)
    start = time.time()
    nb_app=len(app_list)
    print("Starting at time", start)
    while True:
        scheduler_wait()
        print("Remaining timer:", timer-(time.time()-start))
        print("Trying to place an application at:", time.time())
        run_app(app_list[app_id%nb_app])
        app_id+=1


## handle args
app_list=get_app_list(sys.argv[1]) # application list & number of instances
timer=int(sys.argv[2]) # timer
app_info=load_app_info(APP_INFO_FILE)
create_taget_dir(INSTALL_FOLDER)

main()

# vim: tabstop=4 expandtab shiftwidth=4 softtabstop=4
