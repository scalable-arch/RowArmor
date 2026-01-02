# Artifact Evaluation for "RowArmor: Efficient and Comprehensive Protection Against DRAM Disturbance Attacks" (ASPLOS 2026)

## Overview
This repository provides the artifact for the ASPLOS Summer 2026 paper **RowArmor**.

+ List of experiments to reproduce:
  + Security analysis of RowArmor
  + Performance evaluation of RowArmor and other Row Hammer mitigation schemes

+ Structure of this repository:
  + [security_eval](): RowArmor's security evaluation directory (Figure 5, 6, 7, 8)
  + [reliability_eval](): RowArmor's reliability evaluation directory (Table 4)
  + [perf_simulation](./perf_simulation/): RowArmor's performance simulation directory (Figure 9)


## Security Evaluation

This artifact consists of the following components:
+ [RowArmor_security.xlsx](./security_eval/RowArmor_security.xlsx): This file evaluates the security level of each RowHammer defense scheme


## Reliability Evaluation

This artifact consists of the following components:
+ [Fault_sim.cpp](./reliability_eval/Fault_sim.cpp): A Monte Carlo-based error injection simulator
+ [inputs](./reliability_eval/inputs): Ecc input files for the simulation
+ [scripts](./reliability_eval/scripts): Scripts to run the simulation and parse the results
+ [results](./reliability_eval/results): Directory for simulation results

### System specification
No special hardware requirements. Any modern CPU is sufficient.

### Required Dependencies
We tested our evaluation under the following.

+ OS: Ubuntu 18.04.6 LTS (Linux 5.4.0-150-generic)
+ Compiler: gcc/g++  9.4.0 (Ubuntu 9.4.0-1ubuntu1~18.04)
+ Interpreter: Python 3.9.7


### Setting up Configuration Files

To run the reliability simulation, you need to configure error injection and ECC settings. Follow the steps below to set up the necessary configuration files and parameters.

First, set error injection and ECC parameters in [`run.py`](./reliability_eval/run.py).
We provide customizable lists to define OD-ECC status, fault types, and rank-level ECC strengths.
You can reference enums in [`Fault_sim.cpp`](./reliability_eval/Fault_sim.cpp).

```python
oecc  = [0, 1]                        # On-Die ECC: off (0), on (1)
fault = [0, 1, 2, 3, 4, 5, 6, 7, 8]   # Fault types (see details in Fault_sim.cpp)
recc  = [1, 2, 3]                     # Rank-level ECC: AMDCHIPKILL, QPC, OOC 
```

Then, set the number of simulation iterations.
To change the number of fault injections per experiment, edit the following line in [`Fault_sim.cpp`](./reliability_eval/Fault_sim.cpp):

```cpp
#define RUN_NUM 100000000 // line 48
```

### Run the *reliability_eval* 

To run the simulation, use the shell script provided in the `RowArmor/reliability_eval/scripts/` directory.  
The simulation runs Monte Carlo-based error injections by varying ECC and fault parameters.

```bash
$ cd reliability_eval/scripts
$ bash ./sim.sh
```

This script launches multiple simulation processes in parallel and parses the results after the simulations complete.

To launch the simulations and parse the results manually, you can run the following commands.

```bash
cd reliability_eval

# 1. Build the simulator
make

# 2. Run a simulation
./Fault_sim_start <oecc-type> <fault-type> <recc-type> <path-to-output>

# 3. Parse the results
python3 scripts/parse_results.py
```



## Performance Simulation

This artifact consists of the following components:
+ [McSim](./perf_simulation/McSim/): Simulator's back-end
+ [Pthread](./perf_simulation/Pthread): Simulator's front-end
+ [mdfiles](./perf_simulation/mdfiles): Machine description files (mdfiles) for simulation
+ [trace](./perf_simulation/trace): Directory for simualtion trace
+ [runfiles](./perf_simulation/runfiles): Run files for simulation
+ [simulation_scripts](./perf_simulation/simulation_scripts): Simulation scripts for running simulation
+ [results](./perf_simulation/results): Directory for simulation results

### System specification

Since a large number of simulations need to be executed, we recommend running them in parallel on **a dual-socket server with large memory capacity**.
We tested and ran the simulation in the following system specification.

| Hardware | Description | 
|----------|-------------|
| CPU      | 2x Intel Xeon Gold 6338 @2.0 GHz (or equivalent) |
| Memory   | 512 GB DDR4 |


### Required Dependencies
We tested our simulator under the following.

+ OS: Ubuntu 20.04.6 LTS (Linux 5.4.0-214-generic)
+ Compiler: gcc/g++ 11.4.0 (Ubuntu 11.4.0)
+ Tool: [Intel Pin 3.7](https://software.intel.com/sites/landingpage/pintool/downloads/pin-3.7-97619-g0d0c92f4f-gcc-linux.tar.gz)

Also, we need to turn off ASLR for simulation.
```bash
# sudo privilege needed
echo 0 | sudo tee /proc/sys/kernel/randomize_va_space
```

To build the McSimA+ simulator on Linux system, first install the required packages with the following commands:

+ `libelf`: 
```bash
wget https://launchpad.net/ubuntu/+archive/primary/+files/libelf_0.8.13.orig.tar.gz
tar -zxvf libelf_0.8.13.orig.tar.gz
cd libelf-0.8.13.orig/
./configure
make
sudo make install
```

+ `m4`, `elfutils`, `libdwarf`:
```bash
sudo apt-get install -y m4 elfutils libdwarf-dev
```

### Setting up configuration files

First, download the trace files using the given [download_traces.py](./traces/download_traces.py) script.
```bash
# Download the trace files.
cd trace
python3 ./download_traces.py
```

For configuration files, we provide the machine description files (mdfiles) in [mdfiles](./perf_simulation/mdfiles).

For runfiles, we provide the [generate_runfiles.py](./perf_simulation/runfiles/generate_runfiles.py) script to generate simulator runfiles (single, rate, and mix) using a given trace path.

```bash
# Generate simulator configuration files (runfiles)
cd perf_simulation/runfiles
python3 ./generate_runfiles.py -b <path-to-trace>
# Example
python3 ./generate_runfiles.py -b /home/RowArmor/perf_simulation/trace/cpu2017/
```

### Building McSimA+ simulator

McSimA+ simulator utilizes [Intel Pin 3.7](https://www.intel.com/content/www/us/en/developer/articles/tool/pin-a-binary-instrumentation-tool-downloads.html) for its front-end. 

1. Download Intel Pin 3.7 as follows:
```bash
cd perf_simulation
wget https://software.intel.com/sites/landingpage/pintool/downloads/pin-3.7-97619-g0d0c92f4f-gcc-linux.tar.gz 
tar -zxvf pin-3.7-97619-g0d0c92f4f-gcc-linux.tar.gz
# Generate symbolic link for simulator
ln -s "$(pwd)"/pin-3.7-97619-g0d0c92f4f-gcc-linux ./pin
```

2. Build the McSimA+ simulator front-end.
```bash
cd Pthread
make PIN_ROOT="$(pwd)"/../pin obj-intel64/mypthreadtool.so -j
make PIN_ROOT="$(pwd)"/../pin obj-intel64/libmypthread.a
```

3. Build the McSimA+ simulator back-end. 
```bash
cd ../McSim
export PIN="$(pwd)"/../pin/pin
export PINTOOL="$(pwd)"/../Pthread/obj-intel64/mypthreadtool.so
export LD_LIBRARY_PATH=$LD_LIBRARY_PATH:/usr/local/lib
make INCS=-I"$(pwd)"/../pin/extras/xed-intel64/include/xed -j

# or you can source and run given scripts
source setup_mcsim
```

### Running experiments

To directly run the McSimA+ simulation:
```bash
setarch x86_64 -R ./simulator/McSim/obj_mcsim/mcsim -runfile <path-to-runfile> -mdfile <path-to-mdfile> 2>&1 > <path-to-output>
```

We recommend using the provided scripts for running simulations in parallel.

**(Optional) Patch instruction limit for practical simulation**

To ensure that simulations complete within a reasonable time, we provide a helper script that limits the maximum number of executed instructions.

Before generating simulation scripts, you may optionally run:
```bash
# Generate run scripts for simulation
cd ./mdfiles/benign/
python3 patch_max_total_instrs.py
```
This script updates Python configuration files by changing max_total_instrs from 100000000 to 1000000.

We have verified that this change does not affect functional behavior or overall performance trends, and that the qualitative conclusions remain unchanged.

With this limit enabled, the full set of simulations typically completes within one day.

To reproduce the original full-length simulations, simply skip this step.


1. Generate simulation scripts

Inside the simulation_scripts folder:
```bash
# Generate run scripts for simulation
cd ./simulation_scripts
python3 ./generate_simulation_scripts.py -b <path-to-mcsim>
# Example
python3 ./generate_simulation_scripts.py -b /home/RowArmor/perf_simulation/McSim/obj_mcsim/mcsim
```
```bash
python3 ./generate_run_all.py
```
These scripts generate all necessary per-workload run scripts, including run_all_benign.sh.

2. Run all benign simulations
```bash
./run_all_benign.sh
```
This executes all benign simulations using the generated scripts.

3. Handling Failed Simulations (SIG11)

After the initial simulation run, move to the results directory:
```bash
cd ../results
python3 ./rerun.py
```
This script scans all .out files, detects simulations terminated with SIG11, and generates a rerun script:
```bash
../simulation_scripts/rerun_sig11.sh
cd ../simulation_scripts
./rerun_sig11.sh
```
Execute the rerun script:

Proceed only when all simulations finish successfully.


### Parse results

After running simulations, we need to parse the raw results files.
First, we extract and derive IPC from the single process results (single IPCs).
Then, we derive weighted speedups using single IPCs from the rate/mix processes results.

For this, we provide two parsing scripts.

+ [parse_single.py](./perf_simulation/results/parse_single.py): Parse single workloads from the baseline mdfiles.
+ [parse_results.py](./perf_simulation/results/parse_results.py): Parse rate and mix, mix_high workloads from the given single IPCs.

```bash
cd results
python3  ./parse_single.py
```

After parsing single IPCs, we need to modify the [parse_results.py]() scripts.
```bash
vi parse_results.py
# change ipc_list
ipc_list = [ 
    0.803,  0.93,  0.81, 1.027, 0.834, 1.221, 1.394, 1.007,  0.69, 0.607, 
    0.655, 0.606, 0.539, 0.557, 0.419, 1.008, 2.436, 1.691, 1.428, 1.199, 
    1.234, 1.736,  0.43, 0.354, 1.367, 0.704, 0.89, 0.904,
]
```

Then, we can parse the results and generate csv files for graph:
```bash
python3 ./parse_results.py
python3 ./plot_csv.py
python3 ./plot_figure9.py
```

## Contact

For questions or issues, please contact:

Minbok Wi [minbok.wi@scale.snu.ac.kr](mailto:minbok.wi@scale.snu.ac.kr)

Jumin Kim [jumin.kim@scale.snu.ac.kr](mailto:jumin.kim@scale.snu.ac.kr)
