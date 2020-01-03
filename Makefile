## === System configuration begin. === ##
## Executable file suffix: X=.exe for Windows and DOS
X=
B=.sh
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
dfa$X: dfa.c
	$(CC) $(CFLAGS) dfa.c -o dfa$X
nfa$X: nfa.c
	$(CC) $(CFLAGS) nfa.c -o nfa$X
fsc$X: fsc.c
	$(CC) $(CFLAGS) fsc.c -o fsc$X
rex$X: rex.c
	$(CC) $(CFLAGS) rex.c -o rex$X

test: $(Apps)
	$(SH) test$B
clean:
	$(RM) *.ex
	$(RM) *.lg
clobber: clean
	$(RM) $(Apps)
