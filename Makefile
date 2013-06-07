

CC=g++

#CCOPTS=-g
CCOPTS=-O3 

TIME=30

#	$(CC) $(CCOPTS) -o $@ $@.c /lib64/libpthread.so.0 -lm
tsp: tsp.c
	$(CC) $(CCOPTS) -o $@ $@.c -pthread -lm


#
# Small runs which will automatically compute multiple paths to find the minimum
#

run1: tsp
	time tsp final-input-1.txt output1.txt 
	python tsp-verifier.py final-input-1.txt output1.txt

run2: tsp
	time tsp final-input-2.txt output2.txt 
	python tsp-verifier.py final-input-2.txt output2.txt

#
# Larger run which will default to a single iteration
#

run3: tsp
	time tsp final-input-3.txt output3.txt 
	python tsp-verifier.py final-input-3.txt output3.txt

#
# Larger run where we try multiple probs for $(TIME) seconds
# Best in for 5 minutes was "tsp -s 219" = length of 1917421
#
# Invoke with:
#
#     make TIME=20 run3_time
#
run3_time: tsp
	time tsp -v -t $(TIME) final-input-3.txt output3.txt 
	python tsp-verifier.py final-input-3.txt output3.txt

tar:
	tar cvf - tsp.c Makefile | gzip > tsp.tar.gz

clean:
	rm -f *.o *.so core a.out tsp output*.txt tsp.tar.gz

