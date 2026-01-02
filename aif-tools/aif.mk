CFLAGS =

ifneq (, $(shell which dcc))
CC	?= dcc
else
CC	?= clang
CFLAGS += -Wall
endif

EXERCISES	  += aif-tools

SRC = aif-tools_main.c aif-tools.c
INCLUDES = aif.h

# if you add extra .c files, add them here
SRC +=

# if you add extra .h files, add them here
INCLUDES +=


aif-tools:	$(SRC) $(INCLUDES)
	$(CC) $(CFLAGS) $(SRC) -o $@
