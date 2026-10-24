
#
#  Copyright (c) 2023 Peter Hsu.  All Rights Reserved.  See LICENCE file for details.
#

# Cavatools installed in $(CAVA)/bin, $(CAVA)/lib, $(CAVA)/include/cava
ifndef CAVA
CAVA := $(HOME)
endif

ARCH=riscv

#MINUS_O := -g -Ofast -Wswitch
#MINUS_O := -g -Ofast -Wswitch -DDEBUG
MINUS_O := -g -O0 -Wswitch -DDEBUG


HEADERS := options.h opcodes.h caveat.h hart.h

libfiles := options.o instructions.o loader.o decoder.o proxy_syscall.o interpreter.o hart.o
bins := caveat.o gdblink.o $(libfiles)

# Compiling options
CXXFLAGS := -g $(MINUS_O) -I$(CAVA)/include/softfloat

LIBS := $(CAVA)/lib/libsoftfloat.a
LDFLAGS := -Wl,-Ttext=70000000 -static -Wl,--no-relax
#LDFLAGS := -Wl,-Ttext=70000000

caveat: $(bins) $(LIBS)
	$(CXX) $(CXXFLAGS) -o caveat $(bins) $(LDFLAGS) $(LIBS)

# Dependencies

caveat.o instructions.o hart.o interpreter.o: caveat.h opcodes.h options.h
instructions.o: constants.h
interpreter.o:  hart.h semantics.h arithmetic.h
hart.o: hart.h
proxy_syscall.o: ecall_nums.h
gdblink.o:  caveat.h hart.h

# Primary python script making everything

opcodes.h constants.h semantics.h decoder.cc:  ../arch/$(ARCH)/$(ARCH)-isa.def crunch.py
	python crunch.py ../arch/$(ARCH)/$(ARCH)-isa.def

# Python scripts to create various files

ecall_nums.h: Makefile munge_syscalls.py
	python munge_syscalls.py > ecall_nums.h

# Cleanup & installation

clean: tidy
	rm -f *.o *~ ./#*# *.tmp

tidy:
	rm -f caveat opcodes.h constants.h decoder.cc ecall_nums.h semantics.h
	rm -rf $(CAVA)/include/cava
	rm -f $(CAVA)/lib/libcava.a

install: caveat
	cp caveat $(CAVA)/bin/.
	ar r $(CAVA)/lib/libcava.a $(libfiles)
	mkdir -p $(CAVA)/include/cava
	cp $(HEADERS) $(CAVA)/include/cava/.
