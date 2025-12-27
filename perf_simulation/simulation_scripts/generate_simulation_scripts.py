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
    # "mix": [ # mix workload (32 random + 1 high + 1 blend)
    #     "mix.high.py", "mix.blend.py",
    #     "mix.0.py", "mix.1.py", "mix.2.py", "mix.3.py",
    #     "mix.4.py", "mix.5.py", "mix.6.py", "mix.7.py",
    #     "mix.8.py", "mix.9.py", "mix.10.py", "mix.11.py",
    #     "mix.12.py", "mix.13.py", "mix.14.py", "mix.15.py",
    #     "mix.16.py", "mix.17.py", "mix.18.py", "mix.19.py",
    #     "mix.20.py", "mix.21.py", "mix.22.py", "mix.23.py",
    #     "mix.24.py", "mix.25.py", "mix.26.py", "mix.27.py",
    #     "mix.28.py", "mix.29.py", "mix.30.py", "mix.31.py",
    # ]
    "mix": [ # mix workload (32 random + 1 high + 1 blend)
    "mix.high.py",
    "mix.0.py", "mix.1.py", "mix.2.py", "mix.3.py",
    "mix.4.py", "mix.5.py", "mix.6.py", "mix.7.py",
    "mix.8.py", "mix.9.py", "mix.10.py", "mix.11.py",
    "mix.12.py", "mix.13.py", "mix.14.py", "mix.15.py",
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
    # "hydra": ["4096", "2048", "1024", "512"],
    # "blockhammer": ["4096", "2048", "1024", "512", "256", "128", "64"], 
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



def parse():
    parser = OptionParser()
    parser.add_option("-b", "--binary", dest="binary", type="string",
                      help="Absolute path to McSimA+ simulation binary")
    return parser.parse_args()

def write_batched_sh(fname, cmd_list, batch_size=6):

    import os
    with open(fname, 'w') as f:
        f.write("#!/usr/bin/env bash\n\n")

        # run_cmd
        f.write("run_cmd() {\n")
        f.write("    local cmd=\"$1\"\n")
        f.write("    echo \"▶ Running: $cmd\"\n")
        f.write("    eval \"$cmd\" &\n")
        f.write("    echo $!\n")
        f.write("}\n\n")

        # commands
        f.write("commands=(\n")
        for cmd in cmd_list:
            f.write(f'    "{cmd}"\n')
        f.write(")\n\n")

        f.write(f"batch_size={batch_size}\n")
        f.write("total=${#commands[@]}\n\n")

        # batch loop
        f.write("for ((i=0; i<total; i+=batch_size)); do\n")
        f.write("    batch_id=$((i/batch_size+1))\n")
        f.write("    echo \"=== Batch ${batch_id} start (commands ${i} to $((i+batch_size-1))) ===\"\n")
        f.write("    unset pids outfiles cmds\n")
        f.write("    declare -a pids outfiles cmds\n\n")

        
        f.write("    for ((j=0; j<batch_size && i+j<total; j++)); do\n")
        f.write("        cmd=\"${commands[i+j]}\"\n")
        f.write("        cmds[j]=\"$cmd\"\n")
        f.write("        outfiles[j]=\"${cmd##*> }\"\n")
        f.write("        echo \"▶ Launching: $cmd\"\n")
        f.write("        eval \"$cmd\" &\n")
        f.write("        pids[j]=$!\n")
        f.write("    done\n\n")

        
        f.write("    echo \"(sleep 180)\"\n")
        f.write("    sleep 180\n\n")

        
        f.write("    for idx in \"${!pids[@]}\"; do\n")
        f.write("        pid=${pids[idx]}\n")
        f.write("        outfile=${outfiles[idx]}\n")
        f.write("        if grep -q \"C: Tool (or Pin) caused signal 11\" \"$outfile\"; then\n")
        f.write("            echo \"⚠️ Pin error detected in $outfile (PID $pid). Killing it.\"\n")
        f.write("            kill -9 $pid\n")
        f.write("        else\n")
        f.write("            if kill -0 $pid 2>/dev/null; then\n")
        f.write("                wait $pid\n")
        f.write("            fi\n")
        f.write("        fi\n")
        f.write("    done\n\n")

        f.write("    echo \"=== Batch ${batch_id} complete — 'ls' process kill ===\"\n")
        f.write("    killall -9 ls || true\n")
        f.write("done\n")

    os.chmod(fname, 0o755)
    print(f"Generated: {fname}")

def ensure_dir(path):
    if not os.path.exists(path):
        os.makedirs(path, exist_ok=True)


def main():
    options, _ = parse()
    if not options.binary:
        print_help()
        sys.exit(1)

    # Prefix with setarch invocation
    bin_prefix = f"setarch x86_64 -R {options.binary}"
    script_names = []

    # Generate benign.single
    fname = "benign.baseline.single.sh"
    cmds = []
    ensure_dir("../results/baseline/benign")

    for runfile in runfiles["single"]:
        r = f"../runfiles/benign/single/{runfile}"
        m = "../mdfiles/benign/baseline/baseline.py"
        out = f"../results/baseline/benign/baseline.{runfile[:-3]}.out"
        cmds.append(f"{bin_prefix} -runfile {r} -mdfile {m} 2>&1 > {out}")
    write_batched_sh(fname, cmds)

    # benign.rate and benign.mix for each threshold
    for workload in ["rate", "mix"]:
        for md in thresholds:
            for opt in thresholds[md]:
                result_dir = f"../results/{md}/benign"
                ensure_dir(result_dir)
                fname = f"benign.{workload}.{md}.{opt}.sh"
                cmds = []
                for runfile in runfiles[workload]:
                    r = f"../runfiles/benign/{workload}/{runfile}"
                    m = f"../mdfiles/benign/{md}/{md}.{opt}.py"
                    out = f"../results/{md}/benign/benign.{md}.{opt}.{runfile[:-3]}.out"
                    cmds.append(f"{bin_prefix} -runfile {r} -mdfile {m} 2>&1 > {out}")
                write_batched_sh(fname, cmds)

    # malicious.rate and malicious.mix for each threshold
    # for workload in ["rate", "mix"]:
    #     for md in thresholds:
    #         for opt in thresholds[md]:
    #             fname = f"malicious.{workload}.{md}.{opt}.sh"
    #             cmds = []
    #             for runfile in runfiles[workload]:
    #                 r = f"../runfiles/malicious/{workload}/{runfile}"
    #                 m = f"../mdfiles/malicious/{md}/{md}.{opt}.py"
    #                 out = f"../results/malicious/malicious.{md}.{opt}.{runfile[:-3]}.out"
    #                 cmds.append(f"{bin_prefix} -runfile {r} -mdfile {m} 2>&1 > {out}")
    #             write_batched_sh(fname, cmds)

if __name__ == "__main__":
    main()

