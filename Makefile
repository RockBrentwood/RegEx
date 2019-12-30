## === System configuration begin. === ##
## Executable file suffix: X=.exe for Windows and DOS
X=
## Compiler
CC= gcc
## Compiler flags
CFLAGS=-O -std=gnu99
## System shell command
SH=/bin/sh
## System force-remove command
RM=rm -f
## === System configuration end. === ##
Apps=dfa$X nfa$X fsc$X rex$X

all: $(Apps)
dfa:
	$(CC) $(CFLAGS) dfa.c -o dfa$X
nfa:
	$(CC) $(CFLAGS) nfa.c -o nfa$X
fsc:
	$(CC) $(CFLAGS) fsc.c -o fsc$X
rex:
	$(CC) $(CFLAGS) rex.c -o rex$X

test: $(Apps)
	$(SH) test.sh
clean:
	$(RM) *.ex
	$(RM) *.lg
clobber: clean
	$(RM) $(Apps)
