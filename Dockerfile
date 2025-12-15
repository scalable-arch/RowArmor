FROM ubuntu:20.04

ENV DEBIAN_FRONTEND=noninteractive
WORKDIR /root

RUN apt-get update && \
    apt-get install -y \
        build-essential \
        wget \
        curl \
        git \
        python3 \
        python3-pip \
        vim \
        unzip \
        numactl \
        libnuma-dev \
        zlib1g-dev \
        pkg-config \
        libssl-dev \
        libelf-dev \
        autoconf \
        automake \
        bison \
        flex \
        libtool \
        m4 \
        bc \
        elfutils \
        libdwarf-dev \
        && rm -rf /var/lib/apt/lists/*
    
# Turn off ASLR at container runtime
#RUN echo 0 > /proc/sys/kernel/randomize_va_space || true

# Copy
COPY perf_simulation /root/work/perf_simulation
COPY traces /root/work/traces
    
WORKDIR /root/work
    
# Software dependencies
RUN wget https://launchpad.net/ubuntu/+archive/primary/+files/libelf_0.8.13.orig.tar.gz && \
    tar -zxvf libelf_0.8.13.orig.tar.gz && \
    mv -f libelf-0.8.13.orig libelf-0.8.13 && \
    cd libelf-0.8.13 && \
    ./configure && make && make install && \
    cd .. && rm -rf libelf*
    
#RUN wget https://fedorahosted.org/releases/e/l/elfutils/0.161/elfutils-0.161.tar.bz2 && \
#    tar -xvf elfutils-0.161.tar.bz2 && \
#    cd elfutils-0.161 && \
#    ./configure --prefix=/usr && make && make install && \
#    cd .. && rm -rf elfutils*
#RUN apt-get update && apt-get install -y elfutils
    
#RUN git clone git://libdwarf.git.sourceforge.net/gitroot/libdwarf/libdwarf && \
#    cd libdwarf && \
#    ./configure --enable-shared && \
#    make && make install && \
#    cd .. && rm -rf libdwarf
#RUN apt-get update && apt-get install -y libdwarf-dev
    
# Build the simulator
WORKDIR /root/work/perf_simulation
RUN wget https://software.intel.com/sites/landingpage/pintool/downloads/pin-3.7-97619-g0d0c92f4f-gcc-linux.tar.gz && \
    tar -zxvf pin-3.7-97619-g0d0c92f4f-gcc-linux.tar.gz && \
    ln -s $(pwd)/pin-3.7-97619-g0d0c92f4f-gcc-linux ./pin
    
#WORKDIR /root/work/traces
#RUN chmod +x download_traces.py && ./download_traces.py && \
#    tar -zxvf mcsim_cpu2017_traces.tar.gz

WORKDIR /root/work/perf_simulation/Pthread
RUN make PIN_ROOT="$(pwd)"/../pin obj-intel64/mypthreadtool.so -j
RUN make PIN_ROOT="$(pwd)"/../pin obj-intel64/libmypthread.a
    
WORKDIR /root/work/perf_simulation/McSim
ENV PIN=/root/work/perf_simulation/pin/pin
ENV PINTOOL=/root/work/perf_simulation/Pthread/obj-intel64/mypthreadtool.so
ENV LD_LIBRARY_PATH=$LD_LIBRARY_PATH:/usr/local/lib
RUN make INCS=-I/root/work/perf_simulation/pin/extras/xed-intel64/include/xed -j

WORKDIR /root/work/perf_simulation/runfiles
RUN ./generate_runfiles.py

WORKDIR /root/work/perf_simulation/simulation_scripts
RUN ./generate_simulation_scripts.py

WORKDIR /root/work/perf_simulation/
CMD ["/bin/bash"]
