EXE = test_a test_b test_c calc

all: $(EXE)

test_a: test_a.c
	gcc test_a.c -o test_a -lm

test_b: test_b.c
	gcc test_b.c -o test_b -lm

test_c: test_c.c
	gcc test_c.c -o test_c -lm

calc: calc.c
	gcc calc.c -o calc -lm

clean:
	rm -f $(EXE)
