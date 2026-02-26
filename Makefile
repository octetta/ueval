all:
	gcc test_a.c -o test_a -lm
	gcc test_b.c -o test_b -lm
	gcc test_c.c -o test_c -lm
	gcc calc.c -o calc -lm
