import subprocess
import sys

def readElf(exec_path, output_file):
	cmd = ["readelf", "-SW", exec_path]

	try:
		output = subprocess.check_output(cmd, stderr=subprocess.STDOUT)
	except e:
		sys.stderr.write("ERROR: executing readelf, command: " + cmd + "\n")
		sys.stderr.write("Output:\n" + e.output)
		sys.exit()

	with open(output_file, "w+") as f:
		f.write(output)

	return
