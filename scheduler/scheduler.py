#!/usr/bin/python

import time
import subprocess
import sys
import os
import errno
import signal
import shutil
import xmlrpclib

DEBUG = True

# Path Configurations
SCHEDULER_FOLDER = os.getcwd()
HERMIT_INSTALL_FOLDER = "%s/hermit-popcorn/" % os.path.expanduser("~")
PROXY_BIN_X86 = os.path.join(HERMIT_INSTALL_FOLDER, "x86_64-host/bin/proxy")
PROXY_BIN_ARM = os.path.join(HERMIT_INSTALL_FOLDER, "aarch64-hermit/bin/proxy")
BIN_FOLDER = os.path.join(SCHEDULER_FOLDER, "bins")


def get_env(var, default):
    try:
        return os.environ[var]
    except BaseException:
        return default


# Where to run the experiments
EXPERIMENTS_DIR = get_env("HERMIT_EXPERIMENTS_DIR", "/tmp/hermit-scheduler/")

# Machine Configurations
BOARD_NAMES = get_env("HERMIT_BOARD_NAMES", "potato")
BOARD_MEMORY = int(get_env("HERMIT_BOARD_MEMORY", (2 * (2**30))))  # 2GB
BOARD_NB_CORES = int(get_env("HERMIT_BOARD_NB_CORE", 4))
SERVER_NB_CORES = int(get_env("HERMIT_SERVER_NB_CORE", 4))
ONDEMAND_MIGRATION = int(get_env("HERMIT_ON_DEMANDE_MIGRATION", 0))

# Stat variables
start_time = 0
app_count = 0
started = 0
migrated = 0
terminated_local = 0
terminated_remote = 0

# Global variables (see init() function)
# Time of the experiment
timer = -1
# list of applications. example: "ep ep". all application must be in bin_folder
app_list = []
# boards
boards = None

# Applications
# The next two args track running and migrated applications 
# (format: key = pid; value = RApp)
running_app = dict()
migrated_app = dict()


def log(*args):
    if DEBUG:
        print(time.time(), ":", args)



class Machine:
    """Describe a machine: used for boards only (for now)"""

    def __init__(self, name, ident, nb_core, memory_size):
        self.name = name
        self.memory_size = memory_size
        self.free_memory = memory_size
        self.nb_core = nb_core
        self.available_cores = nb_core
        self.ident=ident
        print(self)
    def __str__(self):
        attrs = vars(self)
        return ', '.join("%s: %s" % item for item in attrs.items())

    def __repr__(self):
        return self.__str__()



class Boards:
    """Describe the boards"""
    # TODO: make singleton

    def __init__(self):
        self.boards = []
        self.total_nb_cores = 0
        self.total_available_cores = 0
        self.names=BOARD_NAMES.split()
        self.number=len(self.names)
        print("BOARDS names and num", self.names, self.number)
        self._init_boards()

    def _init_boards(self):
        for i in range(self.number):
            brd = Machine(self.names[i], i, BOARD_NB_CORES, BOARD_MEMORY)
            self.total_nb_cores += brd.nb_core
            self.boards.append(brd)
        self.total_available_cores=self.total_nb_cores
        print self.boards

    def get_available_cores(self):
        return self.total_available_cores

    def get_suitable_board(self, memory):
        if self.total_available_cores == 0:
            return None
        selected = None
        for brd in self.boards:
            if brd.available_cores > 0 and brd.free_memory >= memory:
                selected = brd
                break
        return selected

    #TODO: make these function symetric
    def update_info(self, brd, memory):
        self.total_available_cores -= 1
        brd.available_cores -= 1
        brd.free_memory -= memory
    def remove_proc(self, proc):
        brd=proc.machine
        self.total_available_cores += 1
        brd.available_cores += 1
        brd.free_memory += proc.get_memory_usage(update=False)
        ret = subprocess.call(
                    ["ssh", brd.name, "rm -fr " + proc.dst_dir])
        log("remove remote directory done with retcore", ret)


class RApp:
    """Represent a running application"""
    SCHED_STATUS_FILE = ".status"
    MEMORY_USAGE_FILE = ".memory"
    HW_COUNTER_FILE = ".counter"

    def __init__(self, name, rid, dst_dir, proc):
        self.name = name
        self.rid = rid
        self.dst_dir = dst_dir
        self._update_proc(proc)
        self._start_hw_counter()
        self._start_memory_usage()
        self._start_sched_status()
        self.start_time = time.time()
        self.migrate_time = -1
        self.migration_score = -1
        self.machine=None

    def _update_proc(self, proc):
        self.proc = proc
        self.pid = proc.pid

    def _start_hw_counter(self):
        path = os.path.join(self.dst_dir, self.HW_COUNTER_FILE)
        self.hw_counter_file_path = path
        f = open(path, "w")
        HW_COUNTER_PERIOD = 1000  # in ms
        log("starting perf on", self.dst_dir, self.pid)
        self.monitor = subprocess.Popen(["perf",
                                         "kvm",
                                         "stat",
                                         "-x|",
                                         "-e cache-references",
                                         "-D" + str(HW_COUNTER_PERIOD),
                                         "-I" + str(HW_COUNTER_PERIOD),
                                         "-p",
                                         str(self.pid)],
                                        stdout=f,
                                        stderr=f)

    def _stop_hw_counter(self):
        self.monitor.kill()
        self.hw_counter_usage = -1

    def _start_memory_usage(self):
        self.memory_usage_file_path = path = os.path.join(
            self.dst_dir, self.MEMORY_USAGE_FILE)
        self.memory_usage = -1

    def _start_sched_status(self):
        self.sched_status_file_path = path = os.path.join(
            self.dst_dir, self.SCHED_STATUS_FILE)

    def _get_dir_usage(self):
        total_size = 0
        for dirpath, dirnames, filenames in os.walk(self.dst_dir):
            for f in filenames:
                fp = os.path.join(dirpath, f)
                total_size += os.path.getsize(fp)
        return total_size

    def _get_last_line(self, fn, average_line=1024):
        log("file name is", fn)
        last = None
        try:
            fh = open(fn, 'r')
            lines = fh.readlines()
            if len(lines) > 0:
                last = lines[-1]
        except BaseException:
            pass
        return last

    def _get_int(self, elem):
        try:
            return int(elem)
        except BaseException:
            return -1

    def _get_hw_counter_usage(self):
        log("hwf", self.hw_counter_file_path)
        samples = []
        try:
            fh = open(self.hw_counter_file_path, 'r')
            lines = fh.readlines()
            for line in lines:
                smp = self._get_int(line.split('|')[1])
                if smp > 0:
                    samples.append(smp)
        except BaseException:
            pass
        log("perf samples are", samples)
        if len(samples) < 2:
            return -1
        return sum(samples)/len(samples) #avg

    def get_hw_counter_usage(self, update=True):
        if(update):
            self.hw_counter_usage = self._get_hw_counter_usage()
        return self.hw_counter_usage

    def _get_memory_usage(self):
        line = self._get_last_line(self.memory_usage_file_path)
        if not line:
            return -1
        return self._get_int(line)

    def get_memory_usage(self, update=True):
        if(update):
            self.memory_usage = self._get_memory_usage()
        return self.memory_usage

    def _get_sched_status(self):
        line = self._get_last_line(self.sched_status_file_path)
        if not line:
            return -1
        elms = line.split(':')
        return self._get_int(elms[1])

    def _update_migration_score(self):
        "Return a migration positive integer score (less is better) or -1 (cannot (yet) be migrated) "
        processor_usage = self._get_hw_counter_usage()
        memory_usage = self.get_memory_usage()
        log("perf results", processor_usage)
        log("memory usage", memory_usage)
        if processor_usage == -1 or memory_usage == -1:  # wait until metrics are found
            return -1
        return processor_usage + (memory_usage / 2**20)  # memory usage in MB

    def get_migration_score(self, update=True):
        if update:
            self.migration_score = self._update_migration_score()
        return self.migration_score

    def _stop(self):
        self._stop_hw_counter()

    def migrated(self, proc, machine):
        self._update_proc(proc)
        self._stop()
        self.migrate_time = time.time()
        self.machine=machine

    def terminate(self):
        self._stop()
        end_time = time.time()
        log(self.dst_dir, "Total time", end_time - self.start_time)
        if self.migrate_time != -1:
            log(self.dst_dir, "Total time (remote)", end_time - self.migrate_time)

    def __str__(self):
        attrs = vars(self)
        return ', '.join("%s: %s" % item for item in attrs.items())

    def __repr__(self):
        return self.__str__()

def log_action(action, ddir):
    log(action + " application", ddir)


def log_migration(pid):
    global migrated
    migrated += 1
    log_action("Migrated", migrated_app[pid].dst_dir)


def __terminated(pid, lst, tp):
    ddir = lst[pid].dst_dir
    log_action("Terminated (" + tp + ")", ddir)
    lst[pid].terminate()
    del lst[pid]


def local_terminated(pid):
    global terminated_local
    terminated_local += 1
    __terminated(pid, running_app, "local")


def remote_terminated(pid):
    global terminated_remote
    terminated_remote += 1
    boards.remove_proc(migrated_app[pid])
    __terminated(pid, migrated_app, "remote")


def get_extra_env(resume):
    env = dict()
    env["HERMIT_ISLE"] = "uhyve"
    env["HERMIT_MEM"] = "2G"
    env["HERMIT_CPUS"] = "1"
    env["HERMIT_VERBOSE"] = "0"
    env["HERMIT_MIGTEST"] = "0"
    env["HERMIT_MIGRATE_RESUME"] = str(int(resume))
    env["HERMIT_DEBUG"] = "0"
    env["HERMIT_NODE_ID"] = "0"
    full_checkpoint = str(int(not ONDEMAND_MIGRATION))
    env["HERMIT_FULL_CHKPT_SAVE"] = full_checkpoint
    env["HERMIT_FULL_CHKPT_RESTORE"] = full_checkpoint
    env["ST_AARCH64_BIN"] = "prog_aarch64_aligned"
    env["ST_X86_64_BIN"] = "prog_x86-64_aligned"
    return env


def get_extra_env_str(resume):
    env = get_extra_env(resume)
    env_str = ""
    for k, v in env.items():
        env_str += k + '=' + v + ' '
    return env_str


def __migrate(pid, board):
    proc = running_app[pid]

    # Start checkpoint: send signal
    try:
        os.kill(pid, signal.SIGUSR1)
    except BaseException:
        #The process probably no longer exist
        log("Failed to send signal to", pid, proc)
        return False

    # Wait for checkpoituing to finish
    # w/o on-demande: wait for uhyve to finish
    # FIXME: get info from status FILE!!!!
    log("Waiting for checkpoint")
    rpid, status = os.waitpid(pid, 0)
    log(rpid, "checkpointing done with status",
            status, "return pid", rpid)
    log(rpid, "WIFEXITED", os.WIFEXITED(status),
        "exit code", os.WEXITSTATUS(status))
    log(rpid, "WIFSIGNALED", os.WIFSIGNALED(status))
    log(rpid, "WIFSTOPPED", os.WIFSTOPPED(status))

    log("RSync...")
    # create cmds log file
    f = open("mig.output.txt", "w")

    # Send files: copy the whole repository
    dst_dir = proc.dst_dir
    ret=subprocess.call(["rsync", "-r", dst_dir, board.name +
                     ":" + EXPERIMENTS_DIR], stdout=f, stderr=f)

    log("rsync done with retcore", ret)
    log("SSH...")
    # Run remotly
    ssh_args = ""
    ssh_args += "cd " + dst_dir + "; "
    ssh_args += get_extra_env_str(True) + " ~/proxy ./prog_aarch64_aligned"
    # ssh_args+="echo $!;" #print pid: may be needed if we want to migrate back
    # ssh_args+="wait" #wait for the process to finishes
    new_proc = subprocess.Popen(
        ["ssh", board.name, ssh_args], stdout=f, stderr=f)

    # Update global info
    boards.update_info(board, proc.get_memory_usage())
    # Remove from running app and add to migrated app
    rapp = running_app[pid]
    rapp.migrated(new_proc, board)
    migrated_app[new_proc.pid] = rapp
    log_migration(new_proc.pid)
    del running_app[pid]

    return False


def migrate_sort(kv):
    pid, rapp = kv
    score = rapp.get_migration_score()
    log("Score", pid, score)
    if(score == -1): 
        raise ValueError('Still waiting for info')
    return score


def migrate(running_app):
    # If no core on remote machine
    if boards.get_available_cores() <= 0:
        return False

    # Check-for/Get an arm affine application
    try:
        running_app_sorted = sorted(running_app.items(), key=migrate_sort)
    except BaseException:
        log("Cannot (yet) migrate any application", running_app)
        return False

    # Find a suitable board
    dest = None
    for pid, proc in running_app_sorted:
        dest = boards.get_suitable_board(proc.get_memory_usage())
        if dest:
            best, bproc = pid, proc
            break
    # Try to migrate if any
    if dest:
        log("Best application to migrate is", best, bproc)
        log("Selected board",  dest)
        return __migrate(best, dest)
    return False


def _procs_wait(apps):
    for pid, rapp in apps.items():
        ret = rapp.proc.poll()
        log("proc_wait, pid", pid, "ret code", ret)
        if ret is not None:
            return pid, ret
    return None


def procs_wait(quantum=1):
    ret = _procs_wait(running_app)
    if ret is not None:
        return ret
    ret = _procs_wait(migrated_app)
    if ret is not None:
        return ret
    time.sleep(quantum)
    return None, None


def scheduler_wait():
    # if there still a core on Xeon: continue
    log("number of running apps:", len(running_app))
    if len(running_app) < SERVER_NB_CORES:
        return
    # else try to free a core by migrating
    if migrate(running_app):
        return
    # else: just wait for a core to get freed
    pid, ret_code = procs_wait()
    if pid is None:
        log("Quantum finished with no process finished, pid", pid)
        return
    log("Appplication", pid, "finished with ret code", ret_code)

    if pid in running_app:
        local_terminated(pid)
    elif pid in migrated_app:
        remote_terminated(pid)
    else:
        log("Process", pid, "terminated!")


def run_app(app):
    """ Start an application on the server """
    global started, app_count

    if len(running_app) >= SERVER_NB_CORES:
        return False

    ### Copy foder of the application to EXPERIMENTS_DIR
    src_dir = os.path.join(BIN_FOLDER, app)
    dst_dir = os.path.join(EXPERIMENTS_DIR, app + str(app_count))
    shutil.copytree(src_dir, dst_dir, symlinks=False, ignore=None)

    ### Start application
    os.chdir(dst_dir)
    ori_env = os.environ
    extra_env = get_extra_env(False)
    args=""
    try:
        args=subprocess.check_output(['sh', 'args.sh']).rstrip()
    except BaseException:
        log("no argument file")
    f = open("output.txt", "w")
    proc = subprocess.Popen([PROXY_BIN_X86,
                             "prog_x86-64_aligned", args],
                            env=ori_env.update(extra_env),
                            stdout=f,
                            stderr=f)  # TODO:check error
    # Register application
    running_app[proc.pid] = RApp(app, app_count, dst_dir, proc)
    log("running applications", running_app)
    log_action("Started", dst_dir)
    started += 1
    app_count += 1

    return True

# TODO: delete folders of applications in /tmp/ (ddir)


def print_report():
    end = time.time()
    print("Started applications", started)
    print("Migrated applications", migrated)
    print("Terminated (local) applications", terminated_local)
    print("Terminated (remote) applications", terminated_remote)
    print("Ending at time", end)
    print("Total time is:", end - start_time)


def cleanup():
    def __cleanup(lst, trm):
        for pid, val in lst.items():
            proc = val.proc
            if proc.poll() is not None:
                trm(pid)
            else:
                proc.kill()  # just kill it
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
    global start_time
    set_timer(timer)
    start_time = time.time()
    nb_app = len(app_list)
    print("Starting at time", start_time)
    while True:
        scheduler_wait()
        log("Remaining timer:", timer - (time.time() - start_time))
        log("Trying to place an application at:", time.time())
        run_app(app_list[app_count % nb_app])


def init():
    global timer, app_list, boards
    # Handle assumed files/dirs and check them
    # TODO:
    #   - check that APP_INFO_FILE exist
    #   - check that PROXY_BIN_ARM, BIN_FOLDER exist
    #   - check that EXPERIMENTS_DIR exist in all machines (tmpfs)
    log("NB_CORE_BOARD", BOARD_NB_CORES)
    log("NB_CORE_SERVER", SERVER_NB_CORES)
    log("Default Environ", get_extra_env_str(False))
    # Time of the experiment
    timer = int(sys.argv[2])
    # list of applications. example: "ep ep". all application must be in
    # bin_folder
    app_list = sys.argv[1].split()
    # initialize boards
    boards = Boards()


init()
print("BOARDS", boards)
main()

# vim: tabstop=4 expandtab shiftwidth=4 softtabstop=4
