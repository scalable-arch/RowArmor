#!/usr/bin/python3

import os
from optparse import OptionParser
import csv


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
1.22,0.946,0.754,1.034,0.775,1.348,1.593,1.013,0.587,0.566,0.562,0.588,0.524,0.549,0.563,1.029,2.487,1.584,1.476,1.231,1.275,1.849,0.398,0.421,0.57,0.63,0.99,0.981,
]

mix_trace_list = [
    [1, 3, 10, 25, 13, 4, 22, 23, 12, 26, 24, 8, 9, 27, 2, 11], # mix.high
    [24, 22, 21, 7, 18, 27, 26, 14, 23, 20, 4, 2, 17, 12, 15, 11],
    [22, 13, 9, 11, 19, 12, 18, 6, 21, 2, 16, 26, 15, 17, 1, 24],
    [19, 24, 27, 10, 8, 18, 13, 7, 12, 26, 14, 23, 22, 1, 3, 17],
    [9, 8, 4, 16, 27, 11, 22, 5, 0, 2, 15, 19, 3, 23, 6, 10],
    [7, 2, 9, 25, 3, 8, 23, 4, 16, 22, 18, 24, 5, 20, 1, 10],
    [20, 15, 25, 12, 3, 19, 11, 8, 10, 2, 27, 22, 6, 14, 24, 17],
    [17, 8, 4, 24, 19, 16, 15, 7, 6, 21, 18, 5, 2, 12, 11, 27],
    [17, 7, 1, 14, 13, 24, 23, 4, 11, 15, 0, 10, 6, 2, 16, 19],
    [20, 2, 22, 7, 1, 6, 14, 9, 11, 4, 27, 13, 17, 15, 25, 5],
    [9, 19, 26, 3, 23, 18, 6, 21, 27, 12, 1, 17, 4, 10, 13, 14],
    [15, 14, 19, 22, 12, 10, 23, 27, 20, 16, 4, 25, 11, 0, 24, 7],
    [21, 18, 20, 0, 15, 9, 10, 24, 2, 6, 12, 19, 23, 16, 27, 5],
    [14, 25, 13, 10, 19, 0, 1, 8, 12, 24, 5, 22, 18, 3, 26, 27],
    [6, 2, 18, 23, 27, 11, 0, 7, 1, 26, 16, 25, 12, 9, 5, 22],
    [22, 27, 13, 10, 18, 3, 19, 12, 11, 20, 14, 0, 4, 6, 1, 17],
    [10, 8, 22, 12, 24, 7, 2, 5, 11, 3, 14, 16, 23, 4, 27, 26], # mix.15
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
        "mix.high.py",
        "mix.0.py", "mix.1.py", "mix.2.py", "mix.3.py",
        "mix.4.py", "mix.5.py", "mix.6.py", "mix.7.py",
        "mix.8.py", "mix.9.py", "mix.10.py", "mix.11.py",
        "mix.12.py", "mix.13.py", "mix.14.py", "mix.15.py",
        # "mix.16.py", "mix.17.py", "mix.18.py", "mix.19.py",
        # "mix.20.py", "mix.21.py", "mix.22.py", "mix.23.py",
        # "mix.24.py", "mix.25.py", "mix.26.py", "mix.27.py",
        # "mix.28.py", "mix.29.py", "mix.30.py", "mix.31.py",
    ]
}

thresholds = {
    "baseline": ["none"],
    "rowarmor": ["ooc"],
    "graphene": ["2048", "1024", "512", "256", "128"],
    "para": ["2048", "1024", "512", "256", "128"],
    "para.cube": ["2048", "1024", "512", "256", "128"], 
    "srs.cube": ["2048", "1024", "512"], 
    "srs": ["2048", "1024", "512"], 
    "rampart": ["2048", "1024", "512", "256", "128"], 
    "prac4": ["2048", "1024", "512", "256", "128"], 
    "abacus": ["2048", "1024", "512", "256", "128"], 
}
def parse_one_file(filepath, run_mode, ridx):
    IPC = 0.0
    instr_list = []
    total_instrs = 0
    with open(filepath) as f:
        for line in f:
            if "-- th[" in line:
                ll = line.split()
                if len(ll) >= 5:
                    try:
                        instr_list.append(int(ll[4]))
                    except ValueError:
                        instr_list.append(0)
            if "total number of fetched instructions" in line:
                ll = line.split(" ")
                if len(ll) >= 10:
                    try:
                        total_instrs = int(ll[9])
                    except ValueError:
                        total_instrs = 0
            if "IPC = " in line:
                try:
                    IPC = float(line.split("IPC = ")[1].split(")")[0])
                except Exception:
                    IPC = 0.0

    # 16 cores
    if len(instr_list) < 16:
        instr_list += [0] * (16 - len(instr_list))
    elif len(instr_list) > 16:
        instr_list = instr_list[:16]

    per_core_ws = [0.0] * 16
    if IPC != 0 and total_instrs > 0:
        cycles = float(total_instrs / IPC)
        if run_mode == "rate":
            denom = ipc_list[ridx]
            for i, instr in enumerate(instr_list):
                per_core_ws[i] = (float(instr) / cycles) / denom
        elif run_mode == "mix":
            for i, instr in enumerate(instr_list):
                denom = ipc_list[mix_trace_list[ridx][i]]
                per_core_ws[i] = (float(instr) / cycles) / denom
    return per_core_ws

def build_outfile_index(base_dir):
    
    paths = []
    for root, _, files in os.walk(base_dir, followlinks=True): 
        for fn in files:
            if fn.lower().endswith(".out"):  
                paths.append(os.path.join(root, fn))
    return paths

def parse_filename(fn):
    
    base = os.path.basename(fn)
    tokens = base.split(".")
    if len(tokens) < 5 or tokens[-1] != "out":
        return None
    type_name = tokens[0]
    
    try:
        i = tokens.index("rate")
        run_mode = "rate"
    except ValueError:
        try:
            i = tokens.index("mix")
            run_mode = "mix"
        except ValueError:
            return None
    if i - 1 < 2:
        return None
    config = tokens[i - 1]
    method = ".".join(tokens[1:i - 1])  
    run_base = ".".join(tokens[i:-1])   
    return type_name, method, config, run_mode, run_base

def parse_all(type_name, base_dir, out_csv, per_core):
    out_paths = build_outfile_index(base_dir)

    rows = []
    if per_core:
        header = ["method", "run_mode", "config", "run_base"] + [f"core{i}" for i in range(16)]
    else:
        header = ["method", "run_mode", "config", "run_base", "sum"]
    rows.append(header)

    rate_lookup = {rb: idx for idx, rb in enumerate([r[:-3] for r in runfiles["rate"]])}
    mix_lookup  = {rb: idx for idx, rb in enumerate([r[:-3] for r in runfiles["mix"]])}

    used = 0
    ridx_miss = 0

    for path in out_paths:
        parsed = parse_filename(path)
        if not parsed:
            continue
        tname, method, config, run_mode, run_base = parsed
        if tname != type_name:
            continue

        if run_mode == "rate":
            ridx = rate_lookup.get(run_base)
        else:
            ridx = mix_lookup.get(run_base)
        if ridx is None:
            ridx_miss += 1
            continue

        per_core_ws = parse_one_file(path, run_mode, ridx)
        if per_core:
            rows.append([method, run_mode, config, run_base] + per_core_ws)
        else:
            rows.append([method, run_mode, config, run_base, sum(per_core_ws)])
        used += 1

    
    def _config_key(cfg: str):
        try:
            return (0, int(cfg))   
        except ValueError:
            return (1, cfg.lower())

    data_rows = rows[1:]
    data_rows.sort(key=lambda r: (r[0].lower(), r[1].lower(), _config_key(r[2]), r[3].lower()))
    rows = [rows[0]] + data_rows

    with open(out_csv, "w", newline="") as f:
        csv.writer(f).writerows(rows)
    print(f"[info] saved CSV: {out_csv} (rows={len(rows)-1}, used={used}, ridx_miss={ridx_miss})")


# ===== main =====
def parse():
    parser = OptionParser()
    parser.add_option("-t", "--type", dest="type", type="string", default="benign",
                  help="Type of simulation (benign or malicious)")
    parser.add_option("-d", "--dir", dest="out_dir", type="string", default=".",
                      help="Root directory to recursively search .out files")
    parser.add_option("-o", "--output", dest="output", type="string", default="per_core_simulation_results.csv",
                      help="Output CSV path")
    parser.add_option("--per-core", action="store_true", dest="per_core", default=True,
                      help="Output per-core weighted speedup instead of sum")
    return parser.parse_args()

def main():
    options, _ = parse()
    if options.type not in ("benign", "malicious"):
        print(f"[-] Error: Unsupported type {options.type}")
        return
    parse_all(options.type, options.out_dir, options.output, options.per_core)


if __name__ == "__main__":
    main()