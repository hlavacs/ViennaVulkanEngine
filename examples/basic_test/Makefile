
CC = cl.exe

VERSION = debug

DIR = ../..
INCLUDE = $(DIR)/include
LIB = $(DIR)/bin/$(VERSION)

all: main.exe

.exe.cpp:
	$(CC) -O basic_test.exe /I$(INCLUDE) *.cpp

