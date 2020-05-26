# makefile for GavelScript

OBJS = src/*.cpp # source files to compile
CC = g++ # using GNU C++ compiler

# -w suppresses all warnings (the part that's commented out helps me find memory leaks, it ruins performance though!)
COMPILER_FLAGS = -std=c++17 -o3 -g3 -fsanitize=address

#LINKER_FLAGS specifies the libraries we're linking against (NONE, this is a single header library.)
LINKER_FLAGS = 

#OBJ_NAME specifies the name of our exectuable
OBJ_NAME = bin/Gavel # location of output for build

all:	$(OBJS) 
	$(CC) $(OBJS) $(COMPILER_FLAGS) $(LINKER_FLAGS) -o $(OBJ_NAME)
