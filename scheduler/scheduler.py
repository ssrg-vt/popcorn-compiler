#!/usr/bin/python

#send request to start an application to master.py
import time
import subprocess
import sys, os
import signal
import shutil
import csv
import xmlrpclib

#CONSTANT
SCHEDULER_FOLDER="/home/karaoui/popcorn-xen/popcorn-compiler/scheduler/" #TODO: use env, args, or pwd
PROXY_BIN="/home/karaoui/hermit-popcorn/x86_64-host/bin/proxy"
BIN_FOLDER=os.path.join(SCHEDULER_FOLDER,"bins")
APP_INFO_FILE=os.path.join(SCHEDULER_FOLDER,"info.csv")
RESUME_SCRIPT=os.path.join(SCHEDULER_FOLDER,"resume.sh")
INSTALL_FOLDER="/tmp/test" #TODO: create real tmp folder
BOARD="potato"
BOARD_NB_CORE=2
SERVER_NB_CORES=8

#Global variable
timer=0
app_id=0
app_info=[]
app_list=[]

#TODO:use a class
#format: key=pid; value=(app_name,dest_dir,proc,app_id)
running_app=dict()
migrated_app=dict()

#arguments transforming functions
def load_app_info(file_name):
    af = load_csv(file_name)
    return af
def get_app_list(app_comp):
    return app_comp.split()
def create_taget_dir(dirname):
    os.makedirs(dirname)

#application information (migrate: yes/no) is in the CSV file
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

def __migrate(pid):
    #start checkpoint: send signal
    os.kill(pid, signal.SIGUSR1)

    #wait for checkpoituing to finish
    #w/o on-demande: wait for uhyve to finish
    print("wait signal", pid)
    rpid, status=os.waitpid(pid, 0)
    print("waiting signal done", pid)
    #TODO: check status
    
    #send files: copy the whole repository
    dst_dir=running_app[pid][1]
    subprocess.call(["ssh", BOARD, "mkdir -p", INSTALL_FOLDER])
    print("rsync: before", dst_dir)
    subprocess.call(["rsync", "-r", dst_dir, BOARD+":"+INSTALL_FOLDER])
    print("rsync: after", dst_dir)

    #run remotly: FIXME: don't use the script
    proc=subprocess.Popen([RESUME_SCRIPT, dst_dir]) 

    #remove from running app and add to migrated app
    migrated_app[proc.pid]=(running_app[pid][0],dst_dir,proc,running_app[pid][3])
    del running_app[pid]
    log_action("Migrated", dst_dir)

    return 

def update_runnning_app():
    global running_app
    for pid, val in running_app.items():
        app,ddir,proc,aid=val
        if proc.poll() is not None:
            del running_app[pid]
            log_action("Terminated (local)", ddir)
            #TODO: delete remote folder: ddir


def update_migrated_app():
    global migrated_app
    for pid, val in migrated_app.items():
        app,ddir,proc,aid=val
        if proc.poll() is not None:
            del migrated_app[pid]
            log_action("Terminated (remote)", ddir)
            #TODO: delete remote folder: ddir

def migrate(running_app):
    #update migrated_app
    update_migrated_app()

    #if no core on remote machine
    if BOARD_NB_CORE <= len(migrated_app):
        return False
    #check for an arm affine application
    for pid, val in running_app.items(): 
        app,ddir,proc,aid=val
        if is_board_affine(app):
            __migrate(pid)
            return True
    return False
            

def scheduler_wait():
    #if there still a core on Xeon: continue
    if len(running_app) < SERVER_NB_CORES:
        return
    #else try to free a core by migrating
    if migrate(running_app):
        return
    #else just wait for a core to get freed
    pid, status=os.wait()
    #TODO: check status!

    if pid in running_app:
        ddir=running_app[pid][1]
        log_action("Terminated (local)", ddir)
        del running_app[pid]
    else:
        ddir=migrated_app[pid][1]
        log_action("Terminated (remote)", ddir)
        del migrated_app[pid]

#Start an application on the server
def run_app(app_list, pos):
    if len(running_app) >= SERVER_NB_CORES:
        return False

    app=app_list[0]#TODO: check size

    #copy foder of the application to INSTALL_FOLDER
    src_dir=os.path.join(BIN_FOLDER, app)
    dst_dir=os.path.join(INSTALL_FOLDER, app+str(app_id))
    shutil.copytree(src_dir, dst_dir, symlinks=False, ignore=None)

    #start application
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
    proc=subprocess.Popen([PROXY_BIN, "prog_x86-64_aligned"], env=env) #TODO:check error

    #register application
    running_app[proc.pid]=(app,dst_dir,proc,app_id)
    print("running applications", running_app)
    log_action("Started", dst_dir)

    return True


############################# Initializaiton functions #############
def main():
    global app_id
    nb_app=len(app_list)
    
    start = time.time()
    current = time.time()
    print("Starting at time", start)
    while current-start < timer:
        print("Remaining timer:", timer-(current-start))
        print("Trying to place an application at:", time.time())
        run_app(app_list, app_id%nb_app)
        scheduler_wait()
        app_id+=1
        current = time.time()

    update_runnning_app()
    update_migrated_app()
    end = time.time()
    print("Ending at time", end)
    print("Total time is:", end-start)

## handle args
#TODO: use argparse
app_list=get_app_list(sys.argv[1]) # application list & number of instances
timer=int(sys.argv[2]) # timer
app_info=load_app_info(APP_INFO_FILE)
create_taget_dir(INSTALL_FOLDER)

main()

# vim: tabstop=4 expandtab shiftwidth=4 softtabstop=4
