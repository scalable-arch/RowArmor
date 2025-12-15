import subprocess
import sys
import os

oecc  = [1]
fault = [0, 1, 2, 3, 4, 5, 6, 7, 8]
recc  = [1, 2, 3]

# Create output directory
out_dir = "results"
os.makedirs(f"./{out_dir}", exist_ok=True)

# Launch simulation
print("Running simulations...")
procs = []

for oecc_param in oecc:
    for fault_param in fault:        
        for recc_param in recc:
            procs.append(subprocess.Popen(f"./Fault_sim_start {oecc_param} {fault_param} {recc_param} {out_dir}",
                                        shell=True))

# Wait for all simulations to finish
for p in procs:
    rc = p.wait()
print()
