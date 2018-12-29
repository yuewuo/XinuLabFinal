import os, subprocess, time

# if run failed, try comment out `clear_exiting_network` function, it is only needed when you regenerate network env

"""
running protocal-stack using `sudo ip netns exec <namespace> <command>`
to monitor some interface, run `sudo ip netns exec <namespace> wireshark`
"""

netns = ["ns1", "ns2", "ns3", "ns4"]
vethprefix = "veth_"  # such as "veth_ns1" and "veth_ns1peer"
bridgename = "br1"
ipcnt = 0  # first ip is 10.0.0.1
defaultgw = "10.0.0.0"
def getip():
    global ipcnt
    ipcnt += 1
    return "10.0.0.%d" % ipcnt

def getoutordie(cmd):
    ret, out = subprocess.getstatusoutput(cmd)
    if ret != 0:
        print("command \"%s\" failed, it outputs:" % cmd)
        print((ret, out))
        exit(ret)
    return out

def clear_exiting_network():
    while True:  # delete all the veths
        out = getoutordie("sudo ip link list type veth")
        if out == "":  # delete them all
            break
        veth = out.split('\n')[0].split('@')[0].split(' ')[1]
        print("deleting veth %s" % veth)
        getoutordie("sudo ip link delete %s" % veth)
    print("all veth pair is removed")

    nss = getoutordie("sudo ip netns list").split('\n')
    if len(nss) == 1 and nss[0] == "":
        print("no namespace existed")
    else:
        print("existing namespace:", nss, ", will then delete them")
        getoutordie("sudo ip -all netns delete")
        nss = getoutordie("sudo ip netns list").split('\n')
        if len(nss) > 1 or nss[0] != "":
            print("cannot delete namespace, failed")
            exit(-1)
    
    print("deleting bridge device: \"%s\"" % bridgename)
    ret, out = subprocess.getstatusoutput("sudo ip link set %s down" % bridgename)
    if ret:
        print("delete failed, possibly not exist, so ignore this error")
    else:
        ret, out = subprocess.getstatusoutput("sudo brctl delbr %s" % bridgename)
    print("bridge device \"%s\" is removed" % bridgename)

def generate_network_env():
    getoutordie("sudo brctl addbr %s" % (bridgename))
    getoutordie("sudo ip link set %s up" % (bridgename))
    for ns in netns:
        print("creating namespace %s" % ns)
        getoutordie("sudo ip netns add %s" % ns)
        veth = vethprefix + ns
        vethpeer = vethprefix + ns + "peer"
        print("creating veth pair %s-%s" % (veth, vethpeer))
        getoutordie("sudo ip link add %s type veth peer name %s" % (veth, vethpeer))
        ip = getip()
        print("setting veth %s, ip is %s" % (veth, ip))
        getoutordie("sudo ip link set %s netns %s" % (veth, ns))
        getoutordie("sudo ip netns exec %s ip addr add local %s dev %s" % (ns, ip, veth))
        getoutordie("sudo ip netns exec %s ip link set %s up" % (ns, veth))
        getoutordie("sudo ip netns exec %s ip route add default via %s" % (ns, ip))
        # getoutordie("sudo ip netns exec %s route add default gw %s" % (ns, ip))
        print("setting veth %s" % vethpeer)
        getoutordie("sudo brctl addif %s %s" % (bridgename, vethpeer))
        getoutordie("sudo ip link set %s up" % (vethpeer))

def print_mapping_relation():
    nsmap = {}
    for ns in netns:
        nsmap[ns] = []
    for ele in veths:
        nsmap[ele[0]].append(ele[1])
        nsmap[ele[2]].append(ele[3])
    for ns in netns:
        print("%s:%s" % (ns, "".join([" %s" % e for e in nsmap[ns]])))

if __name__ == "__main__":
    print()
    print("----------------------------------------------------------")
    print("----------- clear exiting network environment ------------")
    print("----------------------------------------------------------")
    print()
    clear_exiting_network()
    print()

    print("----------------------------------------------------------")
    print("------------ creating new network environment ------------") 
    print("----------------------------------------------------------")
    print()
    generate_network_env()
    print()

    print()
    print("network enironment setup success. enjoy it!")
    print()

# veths = [e.split('@')[0].split(' ')[1] for e in getoutordie("ip link list type veth").split('\n')[::2]]
# print("existing veths", veths)
