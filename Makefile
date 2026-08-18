#
#  Copyright (c) 2021 Peter Hsu.  All Rights Reserved.  See LICENCE file for details.
#
#  Environment variables RVTOOLS and CAVA must be defined

# Cavatools installed in $(CAVA)/bin, $(CAVA)/lib, $(CAVA)/include/cava
ifndef CAVA
CAVA := $(HOME)
endif


.PHONY:  nothing clean install
nothing:
	echo "clean, tarball, install?"

install:
	make -C opcodes  install
	make -C caveat   install
	make -C pipesim  install

clean:
	rm -f $(CAVA)/lib/libcava.a *~ ./#*#
	rm -rf $(CAVA)/include/cava 
	rm -f $(CAVA)/bin/caveat
	make -C opcodes   clean
	make -C caveat    clean
	make -C pipesim   clean

tarball:  clean
	( cd ..; tar -czvf cavatools.tgz cavatools )





