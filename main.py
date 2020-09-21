#! /usr/bin/python

import pandas as pd
import numpy as np
from matplotlib import pyplot as plt
from matplotlib import pyplot as plt1




if __name__ == '__main__':
    sample_data = pd.read_csv('results.csv')
    plt.plot(sample_data.threads, sample_data.BQ / 10 ** 6)
    plt.plot(sample_data.threads, sample_data.MSQ / 10 ** 6)
    plt.title(sample_data.batch.iloc[0])
    plt.legend(['BQ', 'MSQ'])
    plt.xlabel('Threads')
    plt.ylabel('Million Operations Per Second')
    plt.savefig(sample_data.name.iloc[0])
    plt.show()


    bars = ('1','2','3','4','8','16','32','64','128')
    y_pos = np.arange(len(bars))
    plt1.bar(y_pos, sample_data.BQ / sample_data.MSQ)
    plt1.xticks(y_pos, bars)
    plt1.title(sample_data.batch.iloc[0])
    plt1.xlabel('Threads')
    plt1.ylabel('BQ throughput / MSQ throughput')
    plt.savefig(sample_data.name.iloc[1])
    plt1.show()







