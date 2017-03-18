all:
	make test && make library

test:
	make -f makefile.test

library:
	make -f makefile.library

clean:
	rm -f *.o *~ core $(INC)/*~
	rm tiresias_tests
        rm libtiresias.a
