# Artifact Evaluation for "RowArmor: Efficient and Comprehensive Protection Against DRAM Disturbance Attacks" (ASPLOS 2026)

## Overview
This repository provides the artifact for the ASPLOS 2026 paper **RowArmor**.

**"RowArmor: Efficient and Comprehensive Protection Against DRAM Disturbance Attacks"**. 

Authors: Yoonyul Yoo (Sungkyunkwan University), Minbok Wi (Seoul National University), Jaeho Shin (Sungkyunkwan University), Yesin Ryu (Samsung Electronics), Yoojin Kim (Sungkyunkwan University), Seonyong Park (Seoul National University), Saeid Gorgin (Sungkyunkwan University), Jung Ho Ahn (Seoul National University), Jungrae Kim (Sungkyunkwan University).

+ List of experiments to reproduce:
  + Security analysis of RowArmor
  + Performance evaluation of RowArmor and other Row Hammer mitigation schemes
  + Reliability analysis for RowArmor

+ Structure of this repository:
  + [security_eval](./security_eval/): RowArmor's security evaluation directory
  + [performance_eval](./performance_eval/): RowArmor's performance simulation directory
  + [reliability_eval](./reliability_eval/): RowArmor's reliability evaluation directory


## Content
- [1. Security Evaluation](#security-evaluation)
- [2. Performance Evaluation](#performance-evaluation)
- [3. Reliability Evaluation](#reliability-evaluation)


## Getting Started

### Clone the artifacts.
We presume that the user's home directory serves as the working directory

   ```bash
   cd ~
   git clone https://github.com/scalable-arch/RowArmor.git
   ```


## 

## Security Evaluation

### System specification
No special hardware requirements. Any modern CPU is sufficient.

### Required Dependencies
We tested our evaluation under the following.
+ OS: Ubuntu 18.04.6 LTS (Linux 5.4.0-150-generic)
+ Compiler: gcc/g++  9.4.0 (Ubuntu 9.4.0-1ubuntu1~18.04)

### Run the security_eval.

  ```bash
   $ cd RowArmor/sim
   $ bash ./sim.sh security [N] [BER]
  ```

- **[N]** : Number of attackers (≤ 128)
- **[BER]** : Bit error rate in percentage (e.g., 0.01, 0.1, 1, 2, 4, 10)   

#### Example Run Steps
Here is a sample execution of the simulation using typical parameters.

- **Reproducing Figure 6 (N<= 16):**  
  Run the following command to simulate the security evaluation with 16 attackers and a bit error rate (BER) of 1%.

  ```bash
  $ cd RowArmor/sim
  $ bash ./sim.sh security 16 0.01
  ```

Below is a snapshot of the output:





## Performance Simulation

This artifact consists of the following components:
+ [McSim](./performance_eval/McSim/): Simulator's back-end
+ [Pthread](./performance_eval/Pthread): Simulator's front-end
+ [mdfiles](./performance_eval/mdfiles): Machine description files (mdfiles) for simulation
+ [runfiles](./performance_eval/runfiles): Run files for simulation
+ [simulation_scripts](./performance_eval/simulation_scripts): Simulation scripts for running simulation
+ [results](./performance_eval/results): Directory for simulation results
+ [traces](./performance_eval/traces): Directory for trace files

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

To build the McSimA+ simulator on Linux system, first install the required packages with the following commands:

+ `libelf`:
```bash
wget https://launchpad.net/ubuntu/+archive/primary/+files/libelf_0.8.13.orig.tar.gz
tar -zxvf libelf_0.8.13.orig.tar.gz
cd libelf-0.8.13.orig/
./configure
make
make install
```

+ `m4`, `elfutils`, `libdwarf`:
```bash
apt-get install -y m4 elfutils libdwarf-dev
```

### Setting up configuration files

First, download the trace files using the given [download_traces.py](./traces/download_traces.py) script.
```bash
# Download the trace files.
cd performance_eval/traces
./download_traces.sh
tar -zxvf mcsim_cpu2017_traces.tar.gz
```

For configuration files, we provide the machine description files (mdfiles) in [mdfiles](./performance_eval/mdfiles).

For runfiles, we provide the [generate_runfiles.py](./performance_eval/runfiles/generate_runfiles.py) for generating runfiles (single, rate, and mix) with a given trace path.
```bash
# Generate simulator's configuration files (runfiles)
cd ../runfiles
./generate_runfiles.py -b <path-to-trace>
```

Lastly, we provide the [generate_simulation_scripts.py](./performance_eval/simulation_scripts/generate_simulation_scripts.py) for generating scripts for running the simulations.
```bash
# Generate run scripts for simulation
cd ../simulation_scripts
./generate_simulation_scripts.py
```

### Building McSimA+ simulator

McSimA+ simulator utilizes [Intel Pin 3.7](https://www.intel.com/content/www/us/en/developer/articles/tool/pin-a-binary-instrumentation-tool-downloads.html) for its front-end. 

1. Download Intel Pin 3.7 as follows:
```bash
cd ../ # performance_eval
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
./build_mcsim.sh
```

### Running experiments

To directly run the McSimA+ simulation:
```bash
setarch x86_64 -R ./simulator/McSim/obj_mcsim/mcsim -runfile <path-to-runfile> -mdfile <path-to-mdfile> 2>&1 > <path-to-output>
```

We recommend to use the scripts for running simulation in parallel.
We provide [runner.py](./performance_eval/simulation_scripts/runner.py) script to run multiple performance simulation in parallel as follows:
```bash
numactl -N {node} -m {memory_node} ./runner.py -p {num_processes} -s <target-simulation-script>
# Trace-driven simulation generates ls processes. We need to kill those processes after finishing the simulation.
killall -9 ls
```

### Parse results

After running simulations, we need to parse the raw results files.
First, we extract and derive IPC from the single process results (single IPCs).
Then, we derive weighted speedups using single IPCs from the rate/mix processes results.

For this, we provide two parsing scripts.

+ [parse_single.py](./performance_eval/results/parse_single.py): Parse single workloads from the baseline mdfiles.
+ [parse_results.py](./performance_eval/results/parse_results.py): Parse rate and mix workloads from the given single IPCs.

```bash
cd results
./parse_single.py
```

After parsing single IPCs, we need to modify the [parse_results.py](./performance_eval/results/parse_results.py) script.
```bash
vi parse_results.py
# change ipc_list
ipc_list = [ 
    0.803,  0.93,  0.81, 1.027, 0.834, 1.221, 1.394, 1.007,  0.69, 0.607, 
    0.655, 0.606, 0.539, 0.557, 0.419, 1.008, 2.436, 1.691, 1.428, 1.199, 
    1.234, 1.736,  0.43, 0.354, 1.367, 0.704, 0.89, 0.904,
]
```

Then, we can parse the results:
```bash
./parse_results.py -t {benign or malicious} -r {runfiles} -m {mdfiles}
```

## Contact

For questions or issues, please contact:
Minbok Wi [minbok.wi@scale.snu.ac.kr](mailto:minbok.wi@scale.snu.ac.kr)




## Reliability Evaluation

### System specification
No special hardware requirements. Any modern CPU is sufficient.

### Required Dependencies
We tested our evaluation under the following.
+ OS: Ubuntu 18.04.6 LTS (Linux 5.4.0-150-generic)
+ Compiler: gcc/g++  9.4.0 (Ubuntu 9.4.0-1ubuntu1~18.04)
+ Interpreter: Python 3.9.7


### Run the *reliability_eval* 

To evaluate reliability, use the Python script provided in the `RowArmor/reliability_eval/` directory.  
The simulation runs Monte Carlo-based error injections by varying ECC and fault parameters.

```bash
$ cd RowArmor/sim
$ bash ./sim.sh reliability 
```

This script launches multiple simulation processes in parallel using the following configure.

### Setting up Configuration Files

To run the reliability simulation, you need to configure error injection and ECC settings. Follow the steps below to set up the necessary configuration files and parameters.

---

#### 1. Set error injection and ECC parameters in [`run.py`](./reliability_eval/run.py)

We provide customizable lists to define OD-ECC status, fault types, and rank-level ECC strengths:

```python
oecc = [0, 1]                        # On-Die ECC: off (0), on (1)
fault = [0, 1, 2, 3, 4, 5, 6, 7, 8]  # Fault types (see details below)
recc = [1, 2, 3]                     # Rank-level ECC: OPC, QPC, AMDCHIPKILL
```

#### Reference enums in [`Fault_sim.cpp`](./reliability_eval/Fault_sim.cpp)

The available parameter values are defined as follows:

```cpp
enum OECC_TYPE {
  OECC_OFF = 0,
  OECC_ON  = 1
};

enum FAULT_TYPE {
  SBE = 0,        // Single-Bit Error
  PIN_1 = 1,      // Single-Pin Error
  SCE = 2,        // Single-Chip Error
  DBE = 3,        // Double-Bit Error
  TBE = 4,        // Triple-Bit Error
  SCE_SBE = 5,    // SCE + SBE
  SCE_DBE = 6,    // SCE + DBE
  SCE_SCE = 7,    // SCE + SCE
  RANK = 8        // Rank-Level Error
};

enum RECC_TYPE {
  RECC_OFF     = 0, // No Rank-level ECC
  OPC          = 1, // Octuple Octet Correcting
  QPC          = 2, // Quadruple Octet Correcting
  AMDCHIPKILL  = 3  // AMD Chipkill
};
```

---

#### 2. Control the number of simulation iterations

To change the number of fault injections per experiment, edit the following line in [`Fault_sim.cpp`](./reliability_eval/Fault_sim.cpp):

```cpp
#define RUN_NUM 100000000 // line 49
```

---

#### 3. Run the reliability simulation

Once configuration is complete, you can run the reliability simulation using:

```bash
cd Reliability
python3 run.py
```


Each combination will launch the following command in parallel:

```bash
./Fault_sim_start <oecc> <fault> <recc> &
```

#### Example Run Steps

Here is a sample execution of the reliability simulation using the default parameter values.

- **Reproducing Table 3:**  
  Run all parameter combinations (`oecc`, `fault`, and `recc`) with the iteration count set to **100 million**.  











