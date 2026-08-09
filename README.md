# cavatools
RISC-V instruction set simulator and performance analysis tools
===============================================================

Cavatools simulates a multi-core RISC-V machine.  The simulator runs on X86 Linux host and presents a user-mode multi-threaded Linux interface to the guest program.  Standard RISC-V tool chain (GNU or LLVM) with GLIBC is used to compile the guest program.  All host Linux system calls are available to the guest program.  Multi-threaded guest programs compiled with Pthead and OpenMP are supported.

###  Getting the sources

The repository is on GitHub:

    $ git clone https://github.com/phaa-eu/cavatools

###  Installation

Environment variable CAVA is installation path, default to home directory $(HOME).  To build Cavatools:

    $ cd cavatools
    $ make install

The following files are installed:

    uspike, caveat, erised in $(CAVA)/bin/
    libcava.a in $(CAVA)/lib/
    several .h files in $(CAVA)/include/cava

Include $(CAVA)/bin in $(PATH) to run Cavatools programs.

###  Running Cavatools

Programs should be compiled -static using the Linux/glibc riscv-gnu-toolchain:

    $ riscv64-unknown-linux-gnu-gcc -static ... testpgm.c -o testpgm

To execute a RISC-V program without simulation:

    $ uspike testpgm <any number of flags and arguments to testpgm>

To see instruction execution performance in real time run in one window:

    $ caveat testpgm <any number of flags and arguments to testpgm>

and this in another window:

    $ erised testpgm

Shared memory segment /dev/shm/caveat (filename can be changed with option) containing counters is created.  Erised passively reads /dev/shm/caveat and displays performance data in its window, controlled interactively by mouse.

