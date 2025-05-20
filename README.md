# RowArmor: Efficient and Comprehensive Protection Against DRAM Disturbance Attacks

## Introduction

This is the code artifact for the paper 
**"RowArmor: Efficient and Comprehensive Protection Against DRAM Disturbance Attacks"**. 

Authors: Yoonyul Yoo (Sungkyunkwan University), Minbok Wi (Seoul National University), Jaeho Shin (Sungkyunkwan University), Yesin Ryu (Samsung Electronics), Yoojin Kim (Sungkyunkwan University), Seonyong Park (Seoul National University), Saeid Gorgin (Sungkyunkwan University), Jung Ho Ahn (Seoul National University), Jungrae Kim (Sungkyunkwan University).

## Contents
- [1. Requirements](#1-requirements)
- [2. Getting Started](#2-getting-started)
- [3. Docker](#3-docker)

## 1. Requirements
**Run-time Environment:**  We suggest using a Linux distribution equipped with a C++-compliant compiler (e.g., g++ 10 or newer) for the reliability and security evaluations. For example, Ubuntu 18.04 or later is sufficient. This artifact has been tested on Ubuntu 18.04.6 LTS and is compatible with most modern Linux systems.

**Security & Reliability Evaluations:**
- **g++** with C++17 support (tested with version 9.4.0)
- **Python3** (tested with version 3.9.7)


## 2. Getting Started

### Clone the artifact.
We presume that the user's home directory serves as the working directory

   ```bash
   cd ~
   git clone https://github.com/scalable-arch/RowArmor.git
   ```


## 

Please run the following steps to regenerate the evaluation:

The evaluation can be easily executed using the ```sim.sh``` script located in the ```RowArmor/sim``` directory.

### run the *security* code.

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



### Run the *Reliability* code

To evaluate reliability, use the Python script provided in the `RowArmor/Reliability/` directory.  
The simulation runs Monte Carlo-based fault injections by varying ECC and fault parameters.

```bash
$ cd RowArmor/sim
$ bash ./sim.sh reliability 
```

This script launches multiple simulation processes in parallel using the following three parameter sets:

**Config Control:**  
- `oecc` – On-Die ECC (e.g., `[0, 1]`)
- `fault` – Fault pattern index (e.g., `[0, 1, 2, ..., 8]`)
- `recc` – Rank-level ECC strength (e.g., `[1, 2, 3]`)

```cpp
enum OECC_TYPE {
  OECC_OFF = 0,
  OECC_ON  = 1
}; // OD-ECC status

enum FAULT_TYPE {
  SBE = 0,        // Single-Bit Error
  PIN_1 = 1,      // Single-Pin Error
  SCE = 2,        // Single-Chip Error
  DBE = 3,        // Double-Bit Error
  TBE = 4,        // Triple-Bit Error
  SCE_SBE = 5,    // SCE + SBE
  SCE_DBE = 6,    // SCE + DBE
  SCE_SCE = 7,    // SCE + SCE
  RANK = 8        // Rank-level error
};

enum RECC_TYPE {
  RECC_OFF     = 0, // No Rank-level ECC
  OPC          = 1, // Octuple Octet Correcting
  QPC          = 2, // Quadruple Octet Correcting
  AMDCHIPKILL  = 3  // AMD Chipkill
};
```
You can modify these values directly in `run.py`:

```python
oecc = [0, 1]
fault = [0, 1, 2, 3, 4, 5, 6, 7, 8]
recc = [1, 2, 3]
```

Each combination will launch the following command in parallel:

```bash
./Fault_sim_start <oecc> <fault> <recc> &
```

**Iteration Control:**  
  To control the number of iterations per simulation, modify the following line in `Fault_sim.cpp`:

```cpp
#define RUN_NUM 100000000 // line 49
```

#### Example Run Steps

Here is a sample execution of the reliability simulation using the default parameter values.

- **Reproducing Table 3:**  
  Run all parameter combinations (`oecc`, `fault`, and `recc`) with the iteration count set to **100 million**.  



## 3. Docker

We provide a pre-built Docker image for convenience.  
You can either download it from Docker Hub or use the link below to load it manually.

#### Option 1. Dokcer Hub

You can also pull the image directly from Docker Hub:
[View on Docker Hub](https://hub.docker.com/r/wogh533/rowarmor)

```bash
$ docker pull wogh533/rowarmor:latest
$ docker run -it wogh533/rowarmor:latest
```


#### Option 2. Download from Google Drive

You can download the Docker image (`rowarmor_image.tar`) from the following link:

[Download Docker Image (rowarmor_image.tar)](https://drive.google.com/uc?export=download&id=1Gz4bWfNjS9mhAaft4xv8FV_BsFAkju4B
)

Once downloaded, run the following commands:

```bash
# Load the Docker image
$ docker load -i rowarmor_image.tar

# Run the container interactively
$ docker run -it --rm rowarmor:latest

```

If you encounter any issues running the image, please open an issue or contact the authors.







