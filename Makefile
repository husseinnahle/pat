CFLAGS = -Wall -Werror=vla -pedantic -std=c11
VAL = --tool=memcheck --leak-check=yes --show-reachable=yes --num-callers=20 --track-fds=yes

compile:
	gcc $(CFLAGS) pat.c -o pat

.PHONY: clean check check_local

clean:
	rm -rf pat

check: pat
	bats check.bats
	
check_local: pat
	bats local.bats
