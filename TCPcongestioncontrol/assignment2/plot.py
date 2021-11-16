#!/usr/bin/python3

import sys
import matplotlib.pyplot as plt

font = {'family': 'serif',
        'weight': 'normal',
        }

with open('t{}.txt'.format(sys.argv[1])) as f:
    lines = f.readlines()
    x = [float(line.split()[0]) for line in lines]

plt.plot(x, linewidth=1)
plt.xlabel('update #')
plt.ylabel('cw size (MSS)')
plt.title('-i {} -m {} -n {} -f {} -s {}'.format(
    sys.argv[2], sys.argv[3], sys.argv[4], sys.argv[5], sys.argv[6]), fontdict=font)
plt.savefig('t{}.png'.format(sys.argv[1]))
