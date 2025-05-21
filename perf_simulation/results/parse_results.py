#!/usr/bin/python3

import os
from optparse import OptionParser

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

ipc_list = [ 
    0.803,  0.93,  0.81, 1.027, 0.834, 1.221, 1.394, 1.007,  0.69, 0.607, 
    0.655, 0.606, 0.539, 0.557, 0.419, 1.008, 2.436, 1.691, 1.428, 1.199, 
    1.234, 1.736,  0.43, 0.354, 1.367, 0.704, 0.89, 0.904,
]

mix_trace_list = [
    [1,5,10,23,2,11,12,22,3,8,24,26,4,9,25,27,], # mix.high
    [0,12,16,20,6,13,17,21,7,14,18,25,10,15,19,27,], # mix.blend
    [19,17,22,11,18,18,23,5,24,26,10,12,20,25,11,9,], # mix.0
    [12,22,6,24,21,27,11,3,13,26,20,12,2,16,22,21,], # mix.1
    [11,12,12,25,23,25,9,21,0,26,11,19,3,14,7,5,], # mix.2
    [25,27,10,21,22,19,14,19,8,25,6,17,2,20,16,19,], # mix.3
    [18,24,1,2,13,20,19,1,21,0,20,6,6,3,9,6,], # mix.4
    [14,23,25,18,10,0,25,19,10,13,18,25,7,17,1,24,],
    [17,23,20,9,27,6,19,5,16,16,16,5,24,21,1,23,],
    [15,22,23,18,14,0,25,16,15,17,24,20,23,11,23,17,],
    [25,18,20,8,8,2,22,4,1,5,10,26,2,6,14,17,],
    [26,8,1,17,4,13,11,19,22,1,13,9,23,4,1,20,],
    [14,11,12,2,6,11,21,21,20,19,8,22,6,6,4,15,],
    [12,19,13,17,8,5,7,22,26,10,12,19,6,25,24,19,],
    [13,5,22,15,19,22,20,10,1,0,17,19,6,20,13,9,],
    [24,17,10,24,12,27,11,7,9,14,18,12,20,12,4,19,],
    [26,8,24,23,7,12,26,22,2,14,22,0,10,2,26,8,],
    [15,16,26,19,12,4,1,27,16,18,12,6,20,14,3,21,],
    [7,9,1,15,25,22,6,11,17,10,8,18,3,21,27,21,],
    [4,16,24,18,21,20,1,20,17,15,9,0,4,4,14,2,],
    [27,20,23,19,15,14,23,23,0,5,25,6,5,7,24,7,],
    [20,1,22,20,25,14,14,22,25,12,21,9,24,10,0,22,],
    [13,11,25,20,3,25,5,2,22,0,9,20,21,27,6,5,],
    [21,6,3,2,5,17,20,12,18,19,7,1,13,7,21,25,],
    [13,10,27,17,14,15,23,6,2,2,2,27,20,21,4,11,],
    [8,18,12,0,0,25,21,7,1,17,17,3,27,20,12,8,],
    [16,17,9,4,22,0,27,27,13,9,15,11,1,24,21,10,],
    [0,24,19,1,2,24,25,1,26,24,11,25,13,19,13,12,],
    [15,5,0,15,21,27,5,6,2,13,6,2,17,20,16,25,],
    [25,5,13,7,19,26,2,7,16,5,13,0,13,16,17,24,],
    [9,3,22,20,10,9,1,22,15,11,26,10,0,24,11,18,],
    [18,10,22,6,3,17,0,22,1,12,22,16,12,1,25,12,],
    [0,20,1,21,19,13,23,14,2,10,19,16,19,14,17,16,],
    [6,2,14,18,21,27,24,11,23,11,1,4,4,3,7,25,],  # mix.31
]

runfiles = {
    "single": [ # single-thread workload (16 workloads, 25 traces)
        "single.500_old.6468.py",  
        "single.505_old.3185.py", "single.505_old.3581.py", "single.505_old.5653.py", "single.505_old.6834.py",
        "single.507_old.6835.py",
        "single.510_old.17633.py",
        "single.511_old.7030.py", 
        "single.519_old.228.py", "single.519_old.5059.py", "single.519_old.7103.py", "single.519_old.13461.py",  
        "single.520_old.3162.py", "single.520_old.6293.py",
        "single.523_old.1214.py", "single.523_old.9188.py",
        "single.525_old.5092.py",
        "single.526_old.6508.py",
        "single.527_old.25724.py",
        "single.531_old.3142.py", 
        "single.541_old.12100.py",  
        "single.548_old.22002.py",  
        "single.549_old.8717.py", "single.549_old.9406.py", "single.549_old.15239.py", "single.549_old.18663.py",
        "single.554_old.1975.py", "single.554_old.21190.py", 
    ],
    "rate": [ # rate workload (16 workloads, 25 traces)
        "rate.500_old.6468.py",  
        "rate.505_old.3185.py", "rate.505_old.3581.py", "rate.505_old.5653.py", "rate.505_old.6834.py",
        "rate.507_old.6835.py",
        "rate.510_old.17633.py",
        "rate.511_old.7030.py", 
        "rate.519_old.228.py", "rate.519_old.5059.py", "rate.519_old.7103.py", "rate.519_old.13461.py",  
        "rate.520_old.3162.py", "rate.520_old.6293.py",
        "rate.523_old.1214.py", "rate.523_old.9188.py",
        "rate.525_old.5092.py",
        "rate.526_old.6508.py",
        "rate.527_old.25724.py",
        "rate.531_old.3142.py", 
        "rate.541_old.12100.py",  
        "rate.548_old.22002.py",  
        "rate.549_old.8717.py", "rate.549_old.9406.py", "rate.549_old.15239.py", "rate.549_old.18663.py",
        "rate.554_old.1975.py", "rate.554_old.21190.py", 
    ],
    "mix": [ # mix workload (32 random + 1 high + 1 blend)
        "mix.high.py", "mix.blend.py",
        "mix.0.py", "mix.1.py", "mix.2.py", "mix.3.py",
        "mix.4.py", "mix.5.py", "mix.6.py", "mix.7.py",
        "mix.8.py", "mix.9.py", "mix.10.py", "mix.11.py",
        "mix.12.py", "mix.13.py", "mix.14.py", "mix.15.py",
        "mix.16.py", "mix.17.py", "mix.18.py", "mix.19.py",
        "mix.20.py", "mix.21.py", "mix.22.py", "mix.23.py",
        "mix.24.py", "mix.25.py", "mix.26.py", "mix.27.py",
        "mix.28.py", "mix.29.py", "mix.30.py", "mix.31.py",
    ]
}

thresholds = {
    "baseline": ["none"],
    "rowarmor": ["qoc", "ooc"],
    "graphene": ["4096", "2048", "1024", "512", "256", "128", "64"],
    "para": ["4096", "2048", "1024", "512", "256", "128", "64"], 
    "cube.para": ["4096", "2048", "1024", "512", "256", "128", "64"], 
    "srs": ["4096", "2048", "1024", "512", "256", "128", "64"], 
    "cube.srs": ["4096", "2048", "1024", "512", "256", "128", "64"], 
    "blockhammer": ["4096", "2048", "1024"],
    "rampart": ["4096", "2048", "1024", "512", "256", "128", "64"], 
    "prac4": ["4096", "2048", "1024", "512", "256", "128", "64"], 
    "hydra": ["4096", "2048", "1024", "512", "256", "128", "64"],
    "abacus": ["4096", "2048", "1024", "512", "256", "128", "64"],
}

def parse_results(type_name, r, m):
    configs = thresholds[m]
    runs = runfiles[r]
    for config in configs:
        weighted_speedups = []
        for ridx, run in enumerate(runs):
            fname = "{}.{}.{}.{}.out".format(type_name, m, config, run[:-3])
            weighted = 0.0
            with open(fname) as f:
                IPC = 0.0
                instr_list = []
                total_instrs = 0
                for line in f:
                    if "-- th[" in line:
                        ll = line.split()
                        instr_list.append(int(ll[4]))
                    if "total number of fetched instructions" in line:
                        ll = line.split(" ")
                        total_instrs = int(ll[9])
                    if "IPC = " in line:
                        IPC = float(line.split("IPC = ")[1].split(")")[0])
                if IPC != 0:
                    cycles = float(total_instrs / IPC)
                    if r == "rate":
                        for idx, instr in enumerate(instr_list):
                            weighted += (float(instr) / cycles) / ipc_list[ridx]
                    elif r == "mix":
                        for idx, instr in enumerate(instr_list):
                            weighted += (float(instr) / cycles) / ipc_list[mix_trace_list[ridx][idx]]
                else:
                    # simulation results: something wrong!
                    weighted = 0.0
                weighted_speedups.append(weighted)
        for weighted_speedup in weighted_speedups:
            print("{},".format(weighted_speedup), end="")
        print()

def parse():
    parser = OptionParser()
    parser.add_option("-t", "--type", dest="type", type="string",
                      help="Type of simulation (benign or malicious)") 
    parser.add_option("-r", "--runfiles", dest="runfiles", type="string",
                      help="Group name of runfiles (rate or mix)")
    parser.add_option("-m", "--mdfiles", dest="mdfiles", type="string",
                      help="Name of mdfiles (baseline, rowarmor, graphene, para, cube.para, srs, cube.srs, rampart, prac4, hydra, and abacus)")
    return parser.parse_args()

def main():
    options, _ = parse()
    if options.type == "benign" or options.type == "malicious":
        parse_results(options.type, options.runfiles, options.mdfiles)
    else:
        print("[-] Error: Unsupported type {}".format(options.type))

if __name__ == "__main__":
    main()
    