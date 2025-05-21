#!/usr/bin/python3

import os

trace_list = [
    "500_old.6468.snappy",
    "505_old.3185.snappy", "505_old.3581.snappy", "505_old.5653.snappy", "505_old.6834.snappy",
    "507_old.6835.snappy",
    "510_old.17633.snappy",
    "511_old.7030.snappy",
    "519_old.228.snappy", "519_old.5059.snappy", "519_old.7103.snappy", "519_old.13461.snappy",
    "520_old.3162.snappy", "520_old.6293.snappy",
    "523_old.1214.snappy", "523_old.9188.snappy",
    "525_old.5092.snappy",
    "526_old.6508.snappy",
    "527_old.25724.snappy",
    "531_old.3142.snappy",
    "541_old.12100.snappy",
    "548_old.22002.snappy",
    "549_old.9406.snappy", "549_old.8717.snappy", "549_old.15239.snappy",  "549_old.18663.snappy",
    "554_old.1975.snappy", "554_old.21190.snappy",
]

def parse_results(fname):
    IPC = 0.0
    with open(fname) as fp:
        instr_list = []
        total_instrs = 0
        for line in fp:
            if "-- th[" in line:
                ll = line.split()
                instr_list.append(int(ll[4]))
            if "total number of fetched instructions" in line:
                ll = line.split(" ")
                total_instrs = int(ll[9])
            if "IPC = " in line:
                IPC = float(line.split("IPC = ")[1].split(")")[0])
    return IPC

def main():
    results = []
    for trace in trace_list:
        fname = "baseline.single.{}.out".format(trace[:-7])
        results.append(parse_results(fname))
    for result in results:
        print("{},".format(result), end="")
    print()
        
if __name__ == "__main__":
    main()
