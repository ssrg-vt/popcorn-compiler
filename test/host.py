''' Utilities for setting up the host for testing. '''

import os
import utils

def hasInterface(ifacesFile, iface):
    ''' Return true if the file contains the interface, or false otherwise. '''
    has = False
    ifacesFile.seek(0)
    for line in ifacesFile:
        if "iface {}".format(iface) in line: has = True
    ifacesFile.seek(0)
    return has

def writeBridgeConfig(ifacesFile, config):
    ifacesFile.seek(0, os.SEEK_END)
    ifacesFile.write("auto {}\n".format(config["iface"]))
    ifacesFile.write("iface {} inet static\n".format(config["iface"]))
    ifacesFile.write("\taddress {}\n".format(config["ip"]))
    ifacesFile.write("\tnetmask {}\n".format(config["netmask"]))
    ifacesFile.write("\tgateway {}\n".format(config["gateway"]))
    ifacesFile.write("\tdns-nameservers {}\n".format(config["dns"]))
    ifacesFile.write("\tbridge_ports {}\n".format(config["net-iface"]))
    ifacesFile.write("\tbridge_stp off\n")
    ifacesFile.write("\tbridge_fd 0\n")
    ifacesFile.write("\tbridge_maxwait 0\n\n")

def createNetworkBridge(ifaces="/etc/network/interfaces",
                        sysctl="/etc/sysctl.conf",
                        networking="/etc/init.d/networking",
                        netIface="eth0",
                        bridgeIface="br0",
                        ipAddr=utils.ipAddr(),
                        netmask="255.255.255.0",
                        gateway="10.1.1.1",
                        dns="8.8.8.8"):
    ''' Set up a network bridge for virtual machines.  Return true if anything
        was modified during setup, or false otherwise.
    '''

    changed = False

    dist = utils.distribution()
    if dist != "Ubuntu" and dist != "Debian":
        utils.die("network bridge setup not supported on {}".format(dist))
    if os.geteuid() != 0: utils.die("must be run as root")

    utils.warn("Creating the network bridge is experimental -- " \
               "you may need to manually adjust!")

    # Add a bridge interface to the system's interfaces file
    with open(ifaces, "r+") as fp:
        if hasInterface(fp, bridgeIface):
            utils.warn("found interface '{}' in {}".format(bridgeIface, ifaces))
        else:
            config = {
                "iface" : bridgeIface,
                "ip" : ipAddr,
                "netmask" : netmask,
                "gateway" : gateway,
                "dns" : dns,
                "net-iface" : netIface
            }
            utils.createBackup(ifaces)
            writeBridgeConfig(fp, config)
            changed = True

    # Turn on IPv4 forwarding
    forwarding = \
        utils.getCommandOutput([ "sysctl", "-n", "net.ipv4.ip_forward" ])
    if forwarding == "0":
        expr = "s/#net.ipv4.ip_forward=1/net.ipv4.ip_forward=1/g"
        replace = [ "sed", "-i.bak", "-e", expr, sysctl ]
        utils.runCmd(replace, wait=True, interactive=True).check_returncode()

        enable = [ "sysctl", "-p", sysctl ]
        utils.runCmd(enable, wait=True, interactive=True).check_returncode()
        changed = True

    # Restart networking
    if changed:
        restart = [ "/etc/init.d/networking", "restart" ]
        utlis.runCmd(restart, wait=True, interactive=True).check_returncode()

    return changed

