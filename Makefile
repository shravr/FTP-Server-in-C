all:objclean ncP

#The following lines contain the generic build options
CC=gcc
CPPFLAGS=
CFLAGS=-g

#These are the options for building the version of the program that uses
#the poll() system call. 
#Add  the specification of any libraries needed to the next lint
POLL_CLIBS=

#List all the .o files that need to be linked for the polled version of the program
#What you need to put on the following line depends upon the .c files required to 
#build the ncP version of the program. Basically take the name of each required .c file 
#and change the .c to .o
POLLSOURCE=ncP.c ncHelper.c usage.c parseOptions.c
POLLOBJS = $(POLLSOURCE:.c=.o)

ncP: $(POLLOBJS) 
	$(CC) -o ncP $(POLLOBJS) $(POLL_CLIBS)

objclean:
	rm -f $(POLLOBJS)

clean:
	rm -f *.o
	rm -f ncP


