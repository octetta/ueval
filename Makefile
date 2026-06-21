EXE = calc
TEST = test_ueval

all: $(EXE)

calc: calc.c ueval.h
	gcc calc.c -o calc -lm

test: $(TEST)
	./$(TEST)

$(TEST): test_ueval.c ueval.h
	gcc -Wall -Wextra test_ueval.c -o $(TEST) -lm

clean:
	rm -f $(EXE) $(TEST)

.PHONY: all test clean
