all:
	gcc -ffast-math -O2 -g -Wall -c json.c
	gcc -ffast-math -O2 -Wconversion -Wunreachable-code -Wstrict-prototypes -Wfloat-equal -Wshadow -Wpointer-arith -Wcast-align -Wformat-nonliteral -Wstrict-overflow=5 -Wwrite-strings -Waggregate-return -Wcast-qual -Wswitch-default -Wswitch-enum -Wformat=2 -g -Wall -isystem. -isystem.. -o longlat2timezone longlat2timezone.c json.o -lm