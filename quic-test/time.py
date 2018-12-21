#!/usr/bin/python
from mininet.topo import Topo
from mininet.net import Mininet
from mininet.cli import CLI
from mininet.link import TCLink
import time
import subprocess
import os,signal
import sys
#   0   1   2   3   4
#h1--r1--r2--r3--r4--h2
# 10.0.X.0
def rp_disable(host):
    ifaces = host.cmd('ls /proc/sys/net/ipv4/conf')
    ifacelist = ifaces.split()    # default is to split on whitespace
    for iface in ifacelist:
       if iface != 'lo': host.cmd('sysctl net.ipv4.conf.' + iface + '.rp_filter=0')

max_queue_size = 100
net = Mininet( cleanup=True )
h1 = net.addHost('h1',ip='10.0.0.1')
r1 = net.addHost('r1',ip='10.0.0.2')
r2 = net.addHost('r2',ip='10.0.1.2')
r3 = net.addHost('r3',ip='10.0.2.2')
r4 = net.addHost('r4',ip='10.0.3.2')
h2 = net.addHost('h2',ip='10.0.4.2')
c0 = net.addController( 'c0' )

net.addLink(h1,r1,intfName1='h1-eth0',intfName2='r1-eth0',cls=TCLink , bw=5, delay='20ms', max_queue_size=10*max_queue_size)
net.addLink(r1,r2,intfName1='r1-eth1',intfName2='r2-eth0',cls=TCLink , bw=5, delay='20ms', max_queue_size=10*max_queue_size)
net.addLink(r2,r3,intfName1='r2-eth1',intfName2='r3-eth0',cls=TCLink , bw=3, delay='10ms', max_queue_size=max_queue_size)
net.addLink(r3,r4,intfName1='r3-eth1',intfName2='r4-eth0',cls=TCLink , bw=5, delay='20ms', max_queue_size=10*max_queue_size)
net.addLink(r4,h2,intfName1='r4-eth1',intfName2='h2-eth0',cls=TCLink , bw=5, delay='20ms', max_queue_size=10*max_queue_size)
net.build()

h1.setIP('10.0.0.1', intf='h1-eth0')
h1.cmd("ifconfig h1-eth0 10.0.0.1 netmask 255.255.255.0")
h1.cmd("route add default gw 10.0.0.2 dev h1-eth0")

r1.cmd("ifconfig r1-eth0 10.0.0.2/24")
r1.cmd("ifconfig r1-eth1 10.0.1.1/24")
r1.cmd("ip route add to 10.0.4.0/24 via 10.0.1.2")
r2.cmd("ip route add to 10.0.0.0/24 via 10.0.0.1")
r1.cmd('sysctl net.ipv4.ip_forward=1')
rp_disable(r1)

r2.cmd("ifconfig r2-eth0 10.0.1.2/24")
r2.cmd("ifconfig r2-eth1 10.0.2.1/24")
r2.cmd("ip route add to 10.0.4.0/24 via 10.0.2.2")
r2.cmd("ip route add to 10.0.0.0/24 via 10.0.1.1")
r2.cmd('sysctl net.ipv4.ip_forward=1')
rp_disable(r2)

r3.cmd("ifconfig r3-eth0 10.0.2.2/24")
r3.cmd("ifconfig r3-eth1 10.0.3.1/24")
r3.cmd("ip route add to 10.0.4.0/24 via 10.0.3.2")
r3.cmd("ip route add to 10.0.0.0/24 via 10.0.2.1")
r3.cmd('sysctl net.ipv4.ip_forward=1')
rp_disable(r3)

r4.cmd("ifconfig r4-eth0 10.0.3.2/24")
r4.cmd("ifconfig r4-eth1 10.0.4.1/24")
r4.cmd("ip route add to 10.0.4.0/24 via 10.0.4.2")
r4.cmd("ip route add to 10.0.0.0/24 via 10.0.3.1")
r4.cmd('sysctl net.ipv4.ip_forward=1')
rp_disable(r4)

h2.setIP('10.0.4.2', intf='h2-eth0')
h2.cmd("ifconfig h2-eth0 10.0.4.2 netmask 255.255.255.0")
h2.cmd("route add default gw 10.0.4.1")

net.start()
time.sleep(1)
client_cmd="./t_sender --log %d --cp %d  --sp %d --duration %d --offset %d "
server_cmd="./t_receiver --log %d --port %d --duration %d --offset %d"
client1_cmd=client_cmd%(1,1234,4321,300000,0)
server1_cmd=server_cmd%(1,4321,310000,0)

#CLI(net)
now=time.time()
second_start=now+30#seconds
client2_cmd=client_cmd%(2,1235,5321,300000,30000)
server2_cmd=server_cmd%(2,5321,310000,30000)

third_start=now+80#seconds
client3_cmd=client_cmd%(3,1236,6321,300000,80000)
server3_cmd=server_cmd%(3,6321,310000,80000)

process=[]
p2=h2.popen(server1_cmd)
p1=h1.popen(client1_cmd)
process.append(p1)
while 1:
    now=time.time()
    if(now>second_start):
        break
    else:
        continue
p4=h2.popen(server2_cmd)
p3=h1.popen(client2_cmd) 
process.append(p3)

while 1:
    now=time.time()
    if(now>third_start):
        break
    else:
        continue
p6=h2.popen(server3_cmd)
p5=h1.popen(client3_cmd) 
process.append(p5)       
while 1:
    time.sleep(1)
    alive=len(process)
    total=alive
    for i in range(total):
        ret=subprocess.Popen.poll(process[i])
        if ret is None:
            continue
        else:
            alive-=1
    if(alive==0):
        break;
    else:
        continue
os.killpg(os.getpgid(p2.pid),signal.SIGTERM)
p2.wait();
os.killpg(os.getpgid(p4.pid),signal.SIGTERM)
p4.wait();
os.killpg(os.getpgid(p6.pid),signal.SIGTERM)
p6.wait();
net.stop()
print "stop"
