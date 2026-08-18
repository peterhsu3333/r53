
CXXFLAGS = -I$(HOME)/include/cava -g

OBJS = pipesim.o display.o
LIBS = $(HOME)/lib/libcava.a 
LDFLAGS = -Wl,-Ttext=70000000 -lncurses

install:  pipesim
	cp pipesim $(HOME)/bin/

clean:
	rm -f *.o *~ ./#*# *.tmp

pipesim:  $(OBJS)
	g++ $(OBJS) $(LIBS) $(LDFLAGS) -o pipesim

pipesim.o:  pipesim.cc display.h
display.o:  display.cc display.h
#	g++ pipesim.cc -o pipesim.o


