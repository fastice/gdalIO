CC =		gcc
CFLAGS =	$(FLAGS) -c -I$(INCLUDEPATH)

OBJS =	gdalIO.o dictionaryCode.o

USER =	$(shell id -u -n)
MACHTYPE = $(shell uname -m)

$(shell mkdir -p $(MACHTYPE)-$(OSTYPE))

all:	$(OBJS)
	mv *.o $(MACHTYPE)-$(OSTYPE)
.KEEP_STATE:
