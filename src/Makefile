DEPENDENCIES = parser-util.c compat.c lib/mpc.c

lispy:
	gcc -std=c99 -Wall lispy.c $(DEPENDENCIES) -o lispy

lispy-debug:
	gcc -g -std=c99 -Wall lispy.c $(DEPENDENCIES) -o lispy-debug