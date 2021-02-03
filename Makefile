
CC      = cl.exe
CCFLAGS = 
RM      = del

default: all

all: vve.lib

.lib.cpp:
	$(CC) *.cpp 

DIRS = \ 
	src \ 
	examples/basic_test 

all: $(DIRS)

$(DIRS):
	pushd $@ & nmake & popd


default: all

all: Helloworld.exe

Helloworld.exe: Helloworld.cpp
	$(CC) $(CCFLAGS) -o Helloworld.exe Helloworld.cpp
	@echo "Build complete"

clean:
	$(RM) *.dSYM *.out Helloworld
	@echo "Clean complete"

