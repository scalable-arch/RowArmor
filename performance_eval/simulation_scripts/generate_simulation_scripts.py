#!/usr/bin/env python3

import sys, os, random
from optparse import OptionParser

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

def print_help():
    print("Usage: ./generate_simulation_scripts.py [OPTION]...")
    print("Generate simulation scripts for McSimA+ simulation")
    print("\t-b, --binary     Absolute path to McSimA+ simulation binary"
        #+ "\t-t, --threshold  Hammer count threshold (4096, 2048, 1024, 512, 256, 128, and 64)"
        #+ "\t-m, --md         Name of mdfiles (baseline, rowarmor, graphene, para, cube.para, srs, cube.srs, rampart, prac4, hydra, and abacus)"
        #+ "\t-r, --run        Group name of runfiles (single, rate, and mix)"
        #+ "\t-t, --type       Type of simulation (benign or malicious)"
        #+ "\t-a, --all        Generate all simulation scripts")
    )

def parse():
    parser = OptionParser()
    parser.add_option("-b", "--binary", dest="binary", type="string",
                      help="Absolute path to McSimA+ simulation binary")
    #parser.add_option("-t", "--threshold", dest="threshold", type="int",
    #                  help="Hammer count threshold (4096, 2048, 1024, 512, 256, 128, and 64)")
    #parser.add_option("-m", "--mdfiles", dest="mdfiles", type="string", 
    #                  help="Name of mdfiles (baseline, rowarmor, graphene, para, cube.para, srs, cube.srs, rampart, prac4, hydra, and abacus)")
    #parser.add_option("-r", "--run", dest="runfiles", type="string",
    #                  help="Group name of runfiles (single, rate, and mix)")
    #parser.add_option("-t", "--type", dest="type", type="string",
    #                  help="Type of simulation (benign or malicious)")
    #parser.add_option("-a", "--all", dest="all", action="store_true",
    #                  help="Generate all simulation scripts")
    return parser.parse_args()
    
def main():
    options, _ = parse()
    binary = "setarch x86_64 -R {}".format(options.binary)
    # benign single (we only include baseline)
    fname = "benign.baseline.single.sh"
    with open(fname, 'w') as f:
        for runfile in runfiles["single"]:
            r = "../runfiles/benign/single/{}".format(runfile)
            m = "../mdfiles/benign/baseline/baseline.py"
            o = "../results/benign/baseline.{}.py".format(runfile[:-3])
            cmd = "{} -runfile {} -mdfile {} 2>&1 > {}\n".format(binary, r, m, o)
            f.write(cmd)
    # benign rate
    for mdfile in thresholds:
        for option in thresholds[mdfile]:
            fname = "benign.rate.{}.{}.sh".format(mdfile, option)
            with open(fname, 'w') as f:
                for runfile in runfiles["rate"]:
                    r = "../runfiles/benign/rate/{}".format(runfile)
                    m = "../mdfiles/benign/{}/{}.{}.py".format(mdfile, mdfile, option)
                    o = "../results/benign.{}.{}.{}.out".format(mdfile, option, runfile[:-3])
                    cmd = "{} -runfile {} -mdfile {} 2>&1 > {}\n".format(binary, r, m, o)
                    f.write(cmd)
    # benign mix
    for mdfile in thresholds:
        for option in thresholds[mdfile]:
            fname = "benign.mix.{}.{}.sh".format(mdfile, option)
            with open(fname, 'w') as f:
                for runfile in runfiles["mix"]:
                    r = "../runfiles/benign/mix/{}".format(runfile)
                    m = "../mdfiles/benign/{}/{}.{}.py".format(mdfile, mdfile, option)
                    o = "../results/benign.{}.{}.{}.out".format(mdfile, option, runfile[:-3])
                    cmd = "{} -runfile {} -mdfile {} 2>&1 > {}\n".format(binary, r, m, o)
                    f.write(cmd)
    # malicious rate
    for mdfile in thresholds:
        for option in thresholds[mdfile]:
            fname = "malicious.rate.{}.{}.sh".format(mdfile, option)
            with open(fname, 'w') as f:
                for runfile in runfiles["rate"]:
                    r = "../runfiles/malicious/rate/{}".format(runfile)
                    m = "../mdfiles/malicious/{}/{}.{}.py".format(mdfile, mdfile, option)
                    o = "../results/malicious.{}.{}.{}.out".format(mdfile, option, runfile[:-3])
                    cmd = "{} -runfile {} -mdfile {} 2>&1 > {}\n".format(binary, r, m, o)
                    f.write(cmd)
    # malicious mix
    for mdfile in thresholds:
        for option in thresholds[mdfile]:
            fname = "malicious.mix.{}.{}.sh".format(mdfile, option)
            with open(fname, 'w') as f:
                for runfile in runfiles["mix"]:
                    r = "../runfiles/malicious/mix/{}".format(runfile)
                    m = "../mdfiles/malicious/{}/{}.{}.py".format(mdfile, mdfile, option)
                    o = "../results/malicious.{}.{}.{}.out".format(mdfile, option, runfile[:-3])
                    cmd = "{} -runfile {} -mdfile {} 2>&1 > {}\n".format(binary, r, m, o)
                    f.write(cmd)

if __name__ == "__main__":
    main()
