# Specify the target file and the install directory
SRC         = src
INC         = include
LIB 				= lib

CLEANEXTS   = o a gcno
CC					= g++
CFLAGS			= -Wall -fexceptions -g -I$(INC) -I/usr/include -fprofile-arcs -ftest-coverage
LIBS				= -L$(LIB) -lgcov -larmadillo -lMvtnorm

_DEPS = divopt.h mvtnorm.h c_util.h c_rand_var_norm.h c_rand_var.h c_logger.h catch.hpp
DEPS = $(patsubst %,$(INC)/%,$(_DEPS))

_OBJ = divopt.o c_util.o c_rand_var_norm.o c_rand_var.o c_logger.o c_tests.o
OBJ = $(patsubst %,%,$(_OBJ))

OUTPUTFILE = divopt

# Default target
.PHONY: all
all: $(OUTPUTFILE)

$(OUTPUTFILE): $(OBJ)
	$(CC) -o $@ $^ $(CFLAGS) $(LIBS)

.PHONY: clean
clean:
	rm -f *.o *~ core $(INC)/*~
	rm -f *.gcno *~ core $(INC)/*~
	rm -f *.gcda *~ core $(INC)/*~
	rm $(OUTPUTFILE)

# Dependencies

%.o: $(SRC)/%.cpp $(DEPS)
	$(CC) $(CFLAGS) -c -o $@ $< $(LIBS)
