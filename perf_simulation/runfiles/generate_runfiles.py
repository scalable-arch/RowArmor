#!/usr/bin/env python3

import sys, os
from optparse import OptionParser

num_cores = 16
num_aggressors = 4

traces = [
    # perlbench_r: Perl interpreter
    "500_old.6468.snappy",
    # mcf_r: Route planning
    "505_old.3185.snappy", "505_old.3581.snappy", "505_old.5653.snappy", "505_old.6834.snappy",
    # cactuBSSN_r: Physics: relativity
    "507_old.6835.snappy",
    # parest_r: Biomedial imaging
    "510_old.17633.snappy",
    # povray_r: Ray tracing
    "511_old.7030.snappy",
    # lbm_r: Fluid dynamics
    "519_old.228.snappy", "519_old.5059.snappy", "519_old.7103.snappy", "519_old.13461.snappy",
    # omnetpp: Discrete event simulation
    "520_old.3162.snappy", "520_old.6293.snappy",
    # xalancbmk_r: xml to html conversion via xslt
    "523_old.1214.snappy", "523_old.9188.snappy",
    # x264_r: Video compression
    "525_old.5092.snappy",
    # blender_r: 3d rendering and animation
    "526_old.6508.snappy",
    # cam4_r: Atmosphere modeling
    "527_old.25724.snappy",
    # deepsjeng_r: Artificial intelligence: alpha-beta tree search (Chess)
    "531_old.3142.snappy",
    # leela_r: Artificial intelligence: Monte Carlo tree search (Go)
    "541_old.12100.snappy",
    # exchange2_r: Artificial intelligence: recursive solution generator (Sudoku)
    "548_old.22002.snappy",
    # fotonik3d_r: Computational electromagnetics
    "549_old.8717.snappy", "549_old.9406.snappy", "549_old.15239.snappy", "549_old.18663.snappy",
    # roms_r: Regional ocean modeling
    "554_old.1975.snappy", "554_old.21190.snappy",
]

# mix_benchmarks = {
#     "mix.high.py": [1, 5, 10, 22, 2, 11, 12, 23, 3, 8, 24, 26, 4, 9, 25, 27],
#     # mix.remove blend.pt
#     # "mix.blend.py": [0, 12, 16, 20, 6, 13, 17, 21, 7, 14, 18, 25, 10, 15, 19, 27],
#     # just random we used for our experiments
#     "mix.0.py": [19, 17, 23, 11, 18, 18, 22, 5, 24, 26, 10, 12, 20, 25, 11, 9],
#     "mix.1.py": [12, 23, 6, 24, 21, 27, 11, 3, 13, 26, 20, 12, 2, 16, 23, 21],
#     "mix.2.py": [11, 12, 12, 25, 22, 25, 9, 21, 0, 26, 11, 19, 3, 14, 7, 5],
#     "mix.3.py": [25, 27, 10, 21, 23, 19, 14, 19, 8, 25, 6, 17, 2, 20, 16, 19],
#     "mix.4.py": [18, 24, 1, 2, 13, 20, 19, 1, 21, 0, 20, 6, 6, 3, 9, 6],
#     "mix.5.py": [14, 22, 25, 18, 10, 0, 25, 19, 10, 13, 18, 25, 7, 17, 1, 24],
#     "mix.6.py": [17, 22, 20, 9, 27, 6, 19, 5, 16, 16, 16, 5, 24, 21, 1, 22],
#     "mix.7.py": [15, 23, 22, 18, 14, 0, 25, 16, 15, 17, 24, 20, 22, 11, 22, 17],
#     "mix.8.py": [25, 18, 20, 8, 8, 2, 23, 4, 1, 5, 10, 26, 2, 6, 14, 17],
#     "mix.9.py": [26, 8, 1, 17, 4, 13, 11, 19, 23, 1, 13, 9, 22, 4, 1, 20],
#     "mix.10.py": [14, 11, 12, 2, 6, 11, 21, 21, 20, 19, 8, 23, 6, 6, 4, 15],
#     "mix.11.py": [12, 19, 13, 17, 8, 5, 7, 23, 26, 10, 12, 19, 6, 25, 24, 19],
#     "mix.12.py": [13, 5, 23, 15, 19, 23, 20, 10, 1, 0, 17, 19, 6, 20, 13, 9],
#     "mix.13.py": [24, 17, 10, 24, 12, 27, 11, 7, 9, 14, 18, 12, 20, 12, 4, 19],
#     "mix.14.py": [26, 8, 24, 22, 7, 12, 26, 23, 2, 14, 23, 0, 10, 2, 26, 8],
#     "mix.15.py": [15, 16, 26, 19, 12, 4, 1, 27, 16, 18, 12, 6, 20, 14, 3, 21],
#     "mix.16.py": [7, 9, 1, 15, 25, 23, 6, 11, 17, 10, 8, 18, 3, 21, 27, 21],
#     "mix.17.py": [4, 16, 24, 18, 21, 20, 1, 20, 17, 15, 9, 0, 4, 4, 14, 2],
#     "mix.18.py": [27, 20, 22, 19, 15, 14, 22, 22, 0, 5, 25, 6, 5, 7, 24, 7],
#     "mix.19.py": [20, 1, 23, 20, 25, 14, 14, 23, 25, 12, 21, 9, 24, 10, 0, 23],
#     "mix.20.py": [13, 11, 25, 20, 3, 25, 5, 2, 23, 0, 9, 20, 21, 27, 6, 5],
#     "mix.21.py": [21, 6, 3, 2, 5, 17, 20, 12, 18, 19, 7, 1, 13, 7, 21, 25],
#     "mix.22.py": [13, 10, 27, 17, 14, 15, 22, 6, 2, 2, 2, 27, 20, 21, 4, 11],
#     "mix.23.py": [8, 18, 12, 0, 0, 25, 21, 7, 1, 17, 17, 3, 27, 20, 12, 8],
#     "mix.24.py": [16, 17, 9, 4, 23, 0, 27, 27, 13, 9, 15, 11, 1, 24, 21, 10],
#     "mix.25.py": [0, 24, 19, 1, 2, 24, 25, 1, 26, 24, 11, 25, 13, 19, 13, 12],
#     "mix.26.py": [15, 5, 0, 15, 21, 27, 5, 6, 2, 13, 6, 2, 17, 20, 16, 25],
#     "mix.27.py": [25, 5, 13, 7, 19, 26, 2, 7, 16, 5, 13, 0, 13, 16, 17, 24],
#     "mix.28.py": [9, 3, 23, 20, 10, 9, 1, 23, 15, 11, 26, 10, 0, 24, 11, 18],
#     "mix.29.py": [18, 10, 23, 6, 3, 17, 0, 23, 1, 12, 23, 16, 12, 1, 25, 12],
#     "mix.30.py": [0, 20, 1, 21, 19, 13, 22, 14, 2, 10, 19, 16, 19, 14, 17, 16],
#     "mix.31.py": [6, 2, 14, 18, 21, 27, 24, 11, 22, 11, 1, 4, 4, 3, 7, 25],
# }

mix_benchmarks  = {
"mix.high.py": [1, 3, 10, 25, 13, 4, 22, 23, 12, 26, 24, 8, 9, 27, 2, 11],
"mix.0.py": [24, 22, 21, 7, 18, 27, 26, 14, 23, 20, 4, 2, 17, 12, 15, 11],
"mix.1.py": [22, 13, 9, 11, 19, 12, 18, 6, 21, 2, 16, 26, 15, 17, 1, 24],
"mix.2.py": [19, 24, 27, 10, 8, 18, 13, 7, 12, 26, 14, 23, 22, 1, 3, 17],
"mix.3.py": [9, 8, 4, 16, 27, 11, 22, 5, 0, 2, 15, 19, 3, 23, 6, 10],
"mix.4.py": [7, 2, 9, 25, 3, 8, 23, 4, 16, 22, 18, 24, 5, 20, 1, 10],
"mix.5.py": [20, 15, 25, 12, 3, 19, 11, 8, 10, 2, 27, 22, 6, 14, 24, 17],
"mix.6.py": [17, 8, 4, 24, 19, 16, 15, 7, 6, 21, 18, 5, 2, 12, 11, 27],
"mix.7.py": [17, 7, 1, 14, 13, 24, 23, 4, 11, 15, 0, 10, 6, 2, 16, 19],
"mix.8.py": [20, 2, 22, 7, 1, 6, 14, 9, 11, 4, 27, 13, 17, 15, 25, 5],
"mix.9.py": [9, 19, 26, 3, 23, 18, 6, 21, 27, 12, 1, 17, 4, 10, 13, 14],
"mix.10.py": [15, 14, 19, 22, 12, 10, 23, 27, 20, 16, 4, 25, 11, 0, 24, 7],
"mix.11.py": [21, 18, 20, 0, 15, 9, 10, 24, 2, 6, 12, 19, 23, 16, 27, 5],
"mix.12.py": [14, 25, 13, 10, 19, 0, 1, 8, 12, 24, 5, 22, 18, 3, 26, 27],
"mix.13.py": [6, 2, 18, 23, 27, 11, 0, 7, 1, 26, 16, 25, 12, 9, 5, 22],
"mix.14.py": [22, 27, 13, 10, 18, 3, 19, 12, 11, 20, 14, 0, 4, 6, 1, 17],
"mix.15.py": [10, 8, 22, 12, 24, 7, 2, 5, 11, 3, 14, 16, 23, 4, 27, 26],
}
def print_help():
    print("Usage: ./generate_runfiles.py [OPTION]...")
    print("Generate runfiles for McSimA+ simulation")
    print("\t-b, --base\tAbsolute path to trace file's directory")

def parse():
    parser = OptionParser()
    parser.add_option("-b", "--base", dest="base", type="string",
                        help="Absolute path to benchmarks directory")
    return parser.parse_args()

def generate_single(basedir):
    if not os.path.exists("./benign/single"):
        os.mkdir("./benign/single")
    for trace in traces:
        fname = "benign/single/single.{}.py".format(trace[:-7])
        with open(fname, 'w') as f:
            f.write("0\t{}{}\t/bin/\tls\n".format(basedir, trace))

def generate_rate(basedir):
    # benign
    if not os.path.exists("./benign/rate"):
        os.mkdir("./benign/rate")
    for trace in traces:
        fname = "benign/rate/rate.{}.py".format(trace[:-7])
        with open(fname, 'w') as f:
            for i in range(num_cores):
                f.write("0\t{}{}\t/bin/\tls\n".format(basedir, trace))
    # malicious
    if not os.path.exists("./malicious/rate"):
        os.mkdir("./malicious/rate")
    for trace in traces:
        fname = "malicious/rate/rate.{}.py".format(trace[:-7])
        with open(fname, 'w') as f:
            for i in range(num_cores - num_aggressors):
                f.write("0\t{}{}\t/bin/\tls\n".format(basedir, trace))

def generate_mix(basedir):
    # benign
    if not os.path.exists("./benign/mix"):
        os.mkdir("./benign/mix")
    for benchmark in mix_benchmarks:
        fname = "benign/mix/{}".format(benchmark)
        with open(fname, 'w') as f:
            for tidx in mix_benchmarks[benchmark]:
                f.write("0\t{}{}\t/bin/\tls\n".format(basedir, traces[tidx]))
    # malicious
    if not os.path.exists("./malicious/mix"):
        os.mkdir("./malicious/mix")
    for benchmark in mix_benchmarks:
        fname = "malicious/mix/{}".format(benchmark)
        with open(fname, 'w') as f:
            for idx, tidx in enumerate(mix_benchmarks[benchmark]):
                if idx < num_aggressors:
                    continue
                else:
                    f.write("0\t{}{}\t/bin/\tls\n".format(basedir, traces[tidx]))

def main():
    options, _ = parse()
    if options.base is None:
        print_help()
        exit(0)
    if not os.path.exists("./benign"):
        os.mkdir("benign")
    if not os.path.exists("./malicious"):
        os.mkdir("malicious")
    generate_single(options.base)
    generate_rate(options.base)
    generate_mix(options.base)

if __name__ == "__main__":
    main()
