#
# Modify this section to point to where stuff is.
# Current names are for a specific file system.
# ROOTDIR: root directory for code (e.g. /home/username directory).
# PROGDIR: location for top source code directory (default ROOTDIR/progs/GIT64)
# BINHOME: root directory for binaries
# BINNAME: archetecture dependent basename for bin dir
# BINDIR: directory for binaries (default BINHOME/bin/BINNAME) (will create if doesn't exist)
# INCLUDEPATH: include path (default PROGDIR anything else could cause a problem)
# Various directors can be overridden with environment variable or from make command
# make BINHOME=/a/path/to/binhome
#
# Base directory for code
USER =	$(shell id -u -n)
$(info  "USER is $(USER)")
MACHTYPE = $(shell uname -m)
OSTYPE = $(shell uname -s)
$(info "MACHTYPE/OSTYPE $(MACHTYPE) $(OSTYPE)")
#
# Default rootdir
ifneq ($(ROOTDIR)),)
	ROOTDIR =	$(dir $(CURDIR))
endif
$(info ROOTDIR="$(ROOTDIR)")
#
# Default root for source code
ifneq ($(PROGDIR)),)
	PROGDIR =       $(dir $(CURDIR))
endif
$(info PROGDIR ="$(PROGDIR)")
#
# Default location root for compiled programs
ifneq ($(BINHOME)),)
	BINHOME =		~$(USER)
endif
# For historical reasons, can compile with 32-bit memory model using MEM=-m32
# In almost all cases, should be compiled as 64bit.
ifneq ($(MEM), -m32)
	BINNAME=	$(MACHTYPE)
	FFTDIR = $(MACHTYPE)-$(OSTYPE)
else
	BINNAME =	i386
	FFTDIR = i386-$(OSTYPE)
endif
#
# Default binary directory
ifneq ($(BINDIR)),)
	BINDIR =	$(BINHOME)/bin/$(BINNAME)
endif
$(info "BINDIR = $(BINDIR)")
#
# Create bin dir if it doesn't exist
$(shell mkdir -p $(BINDIR))
#
# Default include path
ifneq ($(INCLUDEPATH)),)
	INCLUDEPATH =	$(PROGDIR)
endif
$(info INCLUDEPATH ="$(INCLUDEPATH)")
#
# Compiler stuff
#
C =		gcc
#
CFLAGS =	'-O3 $(MEM) -I$(INCLUDEPATH) $(COMPILEFLAGS)'
CCFLAGS =  '-O3 $(MEM) $(COMPILEFLAGS) '
GDAL = -lgdal -lcurl  -lsqlite3 -llzma -lpoppler -lopenjp2 -lssh2 -llcms2
#
CCFLAGS1= -O3 
#-no-pie
# uncomment to debug
#CFLAGS =	'-g $(MEM) -I$(INCLUDEPATH) $(COMPILEFLAGS)'
#CCFLAGS =  '-g $(MEM) -D$(MACHTYPE) $(COMPILEFLAGS)'
#CCFLAGS1= '-g'
#
ifneq ("$(OSTYPE)", "Darwin")
	NOPIE =	-no-pie
	GDAL = -lgdal -lcurl  -lsqlite3 -llzma -lpoppler -lopenjp2 -lssh2 -llcms2
	CFLAGS =	'-O3 $(MEM) -I$(INCLUDEPATH) $(COMPILEFLAGS)'
	CCFLAGS =  '-O3 $(MEM) $(COMPILEFLAGS) '
else
	GDALLIB = /opt/homebrew/lib
	GDALINCLUDE = /opt/homebrew/include
	GDAL = -lgdal -L/opt/homebrew/lib
	CFLAGS =	'-O3 $(MEM) -I$(INCLUDEPATH) $(COMPILEFLAGS) -I$(GDALINCLUDE)'
	CCFLAGS =  '-O3 $(MEM) $(COMPILEFLAGS) -I$(GDALINCLUDE)'
endif
$(info "OS = $(OSTYPE)")
$(info "GDAL = $(GDAL)")
$(info NOPIE ="$(NOPIE)")
#
#
# ******** SHOULD NOT NEED TO MODIFY BELOW HERE *********
#

STANDARD =	$(PROGDIR)/clib/$(MACHTYPE)-$(OSTYPE)/standard.o

UNWRAP = $(PROGDIR)/unwrapSource/unWrap/$(MACHTYPE)-$(OSTYPE)/labelRegions.o

CULLST = $(PROGDIR)/speckleSource/Cullst/$(MACHTYPE)-$(OSTYPE)/cullIslands.o

TARGETS = io

all: $(TARGETS)

TESTIO =	test/$(MACHTYPE)-$(OSTYPE)/testio.o


GDALIO = gdalIO/$(MACHTYPE)-$(OSTYPE)/gdalIO.o gdalIO/$(MACHTYPE)-$(OSTYPE)/dictionaryCode.o
IODIRS =	 gdalIO


io:	
	@for i in ${IODIRS}; do \
		( 	echo "<<< Descending in directory: $$i >>>"; \
	                cd $$i; \
			make FLAGS=$(CCFLAGS) INCLUDEPATH=$(INCLUDEPATH) PAF=0; \
			cd $(PROGDIR); \
		); done





