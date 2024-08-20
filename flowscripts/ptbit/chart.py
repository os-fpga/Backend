# visualize timing path distribution
import argparse
import matplotlib.pyplot as plt

parser = argparse.ArgumentParser()
parser.add_argument("--log", help="PrimeTime log file from ptbit.tcl", default="ptbit.log")
parser.add_argument("--verbose", help="debug", default=False, action="store_true")
args = parser.parse_args()
print('args', args)

f = open(args.log, 'r')
s=None
parse=False
d0=[]
d1=[]
d2=[]
d3=[]
while True:
    l = f.readline()
    if not l:
        break
    if l.startswith('--- PTBIT END PATH CLK2CLK REPORT ---'):
        d=d0
    if l.startswith('--- PTBIT END PATH CLK2OUTPUT REPORT ---'):
        d=d1
    if l.startswith('--- PTBIT END PATH INPUT2CLK REPORT ---'):
        d=d2
    if l.startswith('--- PTBIT END PATH INPUT2OUTPUT REPORT ---'):
        d=d3
    if l.startswith('------------------------------------------------------------------'):
        parse=True
        continue
    if l.startswith('1'):
        parse=False

    if parse:
        c = l.split()
        try:
            d.append(float(c[2])) # delay
        except:
            pass
       #print('endpoint',c[0])
       #print('cell',c[1])
       #print('delay',c[2])
 

fig = plt.figure(figsize=(10,40))
ax0 = fig.add_subplot(5,1,1)
ax1 = fig.add_subplot(5,1,2, sharex=ax0)
ax2 = fig.add_subplot(5,1,3, sharex=ax0)
ax3 = fig.add_subplot(5,1,4, sharex=ax0)
ax4 = fig.add_subplot(5,1,5)

ax0.hist(d0, bins=100, label='CLK2CLK')
ax1.hist(d1, bins=100, label='CLK2OUTPUT')
ax2.hist(d2, bins=100, label='INPUT2CLK')
ax3.hist(d3, bins=100, label='INPUT2OUTPUT')

ax4.plot(d0)
ax4.plot(d1)
ax4.plot(d2)
ax4.plot(d3)

ax3.set_xlabel('ns')
ax0.set_ylabel('CLK2CLK')
ax1.set_ylabel('CLK2OUTPUT')
ax2.set_ylabel('INPUT2CLK')
ax3.set_ylabel('INPUT2OUTPUT')
ax4.set_ylabel('ns')

plt.show()
