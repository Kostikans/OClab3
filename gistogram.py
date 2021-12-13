import json

import numpy as np
from matplotlib import pyplot as plt

razbivka = [1,2,4,8,16,32,64,128,256,512,1024,2048,4096,8192,16384]

def main():
    values = []
    with open("fifo.txt", "r") as f:
        for line in f.readlines():
            values.append(float(line))

    arr = np.empty(len(razbivka), dtype=int)
    print(arr)
    for i in range(len(razbivka)):
        arr[i] = 0

    print(arr)
    for val in values:
        isInf = 0
        for i in range(len(razbivka)):
            if i+1 >= len(razbivka):
                arr[len(razbivka) - 1] +=1
                break
            if val >= razbivka[i] and val < razbivka[i+1]:
                arr[i] +=1
                break

    print(razbivka)
    print(arr)
    # Draw plot

    plt.bar(arr,list(razbivka),width=10)

    plt.xticks(list(razbivka),razbivka,rotation=60)

    plt.ylabel('Count of requests')
    plt.xlabel('req time,ms')
    plt.show()

main()