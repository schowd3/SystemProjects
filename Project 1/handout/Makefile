all: fp 

fp:	fp_program.o fp_functs.o lex.yy.o 
	gcc -o fp fp_program.o fp_functs.o lex.yy.o -lfl -lm

fp_functs.o:	fp_functs.c fp.h
	gcc -g -c fp_functs.c	

fp_program.o:	fp_program.c fp_parse.h
	gcc -c fp_program.c	

lex.yy.o:	lex.yy.c fp_parse.h
	gcc -c lex.yy.c

lex.yy.c:	fp.l
	flex fp.l

clean:
	rm -rf lex.yy.c lex.yy.o fp_program.o fp_functs.o 

reallyclean:
	rm -rf fp lex.yy.c lex.yy.o fp_program.o fp_functs.o 
