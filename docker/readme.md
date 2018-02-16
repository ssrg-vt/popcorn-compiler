- Use the `docker-build.sh` script rather than calling directly the docker 
building command.
- If your system is configured to only execute docker as root, you need to edit
  the script and replace the call to `docker` by `sudo docker`

Usage:

```bash
./docker-build.sh
# It takes a bit of time ...
docker run -t -i popcorn-compiler /bin/bash
root@xxx:~# ls popcorn-compiler/
APPLICATIONS   INSTALL  README                 common  install_compiler.py  patches  tutorial
BUG_REPORTING  ISSUES   binutils-2.27.tar.bz2  docker  lib                  tool     util
```
