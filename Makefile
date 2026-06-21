EXE = calc
TEST = test_ueval
BENCH = bench_ueval

all: $(EXE)

calc: calc.c ueval.h
	gcc calc.c -o calc -lm

test: $(TEST)
	./$(TEST)

$(TEST): test_ueval.c ueval.h
	gcc -Wall -Wextra test_ueval.c -o $(TEST) -lm

bench: $(BENCH)
	./$(BENCH)

$(BENCH): bench_ueval.c ueval.h
	gcc -O2 bench_ueval.c -o $(BENCH) -lm

clean:
	rm -f $(EXE) $(TEST) $(BENCH)

.PHONY: all test bench clean
