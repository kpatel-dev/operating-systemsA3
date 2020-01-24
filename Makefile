#Makefile for Assignment3
default: asn3.out

asn3: asn3.out

asn3.out: assignment3.c 
	cc -o asn3.out assignment3.c -lpthread

clean:
	rm *.out assignment_3_output_file.txt