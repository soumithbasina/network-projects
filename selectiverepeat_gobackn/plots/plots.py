import matplotlib.pyplot as plt

font = {'family': 'serif',
        'weight': 'normal',
        }

x = [0.2, 0.1, 0.01, 0.001, 0.00001]

# go back n numbers
yrtt = [3.534, 1.14, 0.48, 0.47, 0.45]
# ytr = [1.18, 1.02, 1.005, 1.003]

# selective repeat numbers
# yrtt = [4.89, 1.62, 0.852, 0.851]

plt.plot(x, yrtt)
# plt.plot(x, ytr)
plt.xlabel('random_drop_prob')
# plt.ylabel('Retransmission ratio')
plt.ylabel('Round Trip Time (RTT) (ms)')
plt.title('Packet gen. rate: 20')
plt.xscale('log')
plt.show()
