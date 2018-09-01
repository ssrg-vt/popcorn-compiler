# Scheduler Arguments:
The scheduler take two arguments:
1) A timer (integer) : time of the experiment in seconds
2) A list of applications (space separated list). Example: "ep cg"


# Implicit input:
The scheduler requires the following:
- The "bins/" folder: contains a sub-folder for each application For an example
  look at the prepare.sh file. This latter creates all the binaries of the NPB
applications

- An "info.csv" file that contains for each application a binary (0/1) for
  wether we can migrate an applicaiton or not. example of content with two
application ep and cg (Note: avoid using spaces):
```
Benchmark,Migrate
ep,1
cg,0
```

- Number of cores: can be controlled by environment variables
  HERMIT_SERVER_NB_CORE (default 2) and HERMIT_BOARD_NB_CORE (default 2).

-There other configuration that are for know coded as constants at the begining
of the python scripts. Like the board name which is called: "potato".

# launching the scheduler
see start_scheduler.sh


# Generated files/directory
- "/tmp/hermit-scheduler": a folder containing runned binaries
	The same folder is also created on the board
