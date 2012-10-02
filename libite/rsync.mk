# Unittest makefile for rsync.c
.PHONY: clean

SRC      = rsync.c
OBJS     = dir.o copyfile.o fexist.o fisdir.o fmode.o
UNIT     = rsync.unittest
#CPPFLAGS = -DDEBUG -I../../.. -I../../include

all: build run

clean:
	@rm -f $(OBJS) $(UNIT)

# This rule also forces a cleanup of built objects to not leave any object files
# lingering in the tree to interfere with cross builds.
build: clean $(OBJS)
	$(CC) $(CFLAGS) $(CPPFLAGS) -o $(UNIT) -DUNITTEST $(SRC) $(OBJS)
	@rm -f $(OBJS)

run:
	@./$(UNIT)

