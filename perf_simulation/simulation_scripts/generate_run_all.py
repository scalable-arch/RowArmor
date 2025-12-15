#!/usr/bin/env python3
import os

OUTPUT = "run_all_benign.sh"

def main():
    
    benign_files = [
        f for f in os.listdir(".")
        if f.startswith("benign") and f.endswith(".sh")
    ]
    benign_files.sort()

    with open(OUTPUT, "w") as f:
        f.write("#!/usr/bin/env bash\n")
        f.write("cd ../Pthread\n")
        f.write('make PIN_ROOT="$(pwd)"/../pin obj-intel64/mypthreadtool.so -j\n')
        f.write('make PIN_ROOT="$(pwd)"/../pin obj-intel64/libmypthread.a\n')
        f.write("cd ../McSim\n")
        f.write("source setup_mcsim\n")
        f.write("cd ../simulation_scripts\n\n")

        # benign*.sh 
        for sh in benign_files:
            f.write(f"./{sh}\n")

        f.write("\nwait\n")
        f.write("sleep 10\n")
        f.write("killall -9 ls\n\n")
        f.write('echo\n')
        f.write('echo "All done."\n')

    os.chmod(OUTPUT, 0o755)
    print(f"Generated: {OUTPUT}, benign {len(benign_files)} files included.")


if __name__ == "__main__":
    main()
