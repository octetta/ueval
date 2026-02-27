EXE = calc

all: $(EXE)

calc: calc.c ueval.h
	gcc calc.c -o calc -lm

clean:
	rm -f $(EXE)
