MODULE = qdtools
SPEC = smartmet-qdtools

MAINFLAGS = -MD -Wall -W -Wno-unused-parameter -fno-omit-frame-pointer

ifeq (6, $(RHEL_VERSION))
  MAINFLAGS += -std=c++0x
else
  MAINFLAGS += -std=c++11 -fdiagnostics-color=always
endif

# mdsplib does not declare things correctly

MAINFLAGS += -fpermissive


EXTRAFLAGS = \
	-Werror \
	-Winline \
	-Wpointer-arith \
	-Wcast-qual \
	-Wcast-align \
	-Wwrite-strings \
	-Wno-pmf-conversions \
	-Wchar-subscripts \
	-Woverloaded-virtual

DIFFICULTFLAGS = \
	-Wunreachable-code \
	-Wconversion \
	-Wsign-promo \
	-Wnon-virtual-dtor \
	-Wctor-dtor-privacy \
	-Wredundant-decls \
	-Weffc++ \
	-Wold-style-cast \
	-pedantic \
	-Wshadow

# Default compiler flags

DEFINES = -DUNIX

CFLAGS = $(DEFINES) -O2 -g -DNDEBUG $(MAINFLAGS)
LDFLAGS = 

# Special modes

CFLAGS_DEBUG = $(DEFINES) -O0 -g $(MAINFLAGS) $(EXTRAFLAGS) -Werror
CFLAGS_PROFILE = $(DEFINES) -O2 -g -pg -DNDEBUG $(MAINFLAGS)

LDFLAGS_DEBUG =
LDFLAGS_PROFILE =

# Boost 1.69

ifneq "$(wildcard /usr/include/boost169)" ""
  INCLUDES += -isystem /usr/include/boost169
  LIBS += -L/usr/lib64/boost169
endif

# gdal 30 from pgdg

ifneq "$(wildcard /usr/gdal30/include)" ""
  INCLUDES += -isystem /usr/gdal30/include
  LIBS += -L/usr/gdal30/lib
else
  INCLUDES += -isystem /usr/include/gdal
endif

INCLUDES +=  \
	-isystem $(includedir)/netcdf-3 \
	-isystem $(includedir)/bufr \
	-isystem $(includedir)/libecbufr \
	-isystem $(includedir)/ecbufr \
	-I$(includedir)/smartmet

LIBS += -L$(libdir) \
	-lsmartmet-calculator \
	-lsmartmet-smarttools \
	-lsmartmet-newbase \
	-lsmartmet-macgyver \
	-lsmartmet-gis \
	-lsmartmet-imagine \
	-lboost_regex \
	-lboost_date_time \
	-lboost_program_options \
	-lboost_iostreams \
	-lboost_thread \
	-lboost_filesystem \
        -lboost_system \
	-lgdal \
	-lmetar \
	-ljasper \
	-leccodes \
	-lnetcdf_c++ -lnetcdf \
	-lMXADataModel -lhdf5 \
	-lbufr \
	-lecbufr \
	-lfmt \
	-lbz2 -ljpeg -lpng -lz -lrt \
	-lpthread

# Common library compiling template

# Installation directories

processor := $(shell uname -p)

ifeq ($(origin PREFIX), undefined)
  PREFIX = /usr
else
  PREFIX = $(PREFIX)
endif

ifeq ($(processor), x86_64)
  libdir = $(PREFIX)/lib64
else
  libdir = $(PREFIX)/lib
endif

objdir = obj
includedir = $(PREFIX)/include

ifeq ($(origin BINDIR), undefined)
  bindir = $(PREFIX)/bin
else
  bindir = $(BINDIR)
endif

ifeq ($(origin DATADIR), undefined)
  datadir = $(PREFIX)/share
else
  datadir = $(DATADIR)
endif

# Special modes

ifneq (,$(findstring debug,$(MAKECMDGOALS)))
  CFLAGS = $(CFLAGS_DEBUG)
  LDFLAGS = $(LDFLAGS_DEBUG)
endif

ifneq (,$(findstring profile,$(MAKECMDGOALS)))
  CFLAGS = $(CFLAGS_PROFILE)
  LDFLAGS = $(LDFLAGS_PROFILE)
endif

# Compilation directories

vpath %.cpp source main
vpath %.h include
vpath %.o $(objdir)
vpath %.d $(objdir)

# How to install

INSTALL_PROG = install -m 775
INSTALL_DATA = install -m 664

# The files to be compiled

HDRS = $(patsubst include/%,%,$(wildcard *.h include/*.h))

MAINSRCS     = $(patsubst main/%,%,$(wildcard main/*.cpp))
MAINPROGS    = $(MAINSRCS:%.cpp=%)
MAINOBJS     = $(MAINSRCS:%.cpp=%.o)
MAINOBJFILES = $(MAINOBJS:%.o=obj/%.o)

SRCS     = $(patsubst source/%,%,$(wildcard source/*.cpp))
OBJS     = $(SRCS:%.cpp=%.o)
OBJFILES = $(OBJS:%.o=obj/%.o)

INCLUDES := -Iinclude $(INCLUDES)

# For make depend:

ALLSRCS = $(wildcard main/*.cpp source/*.cpp)

.PHONY: test rpm

# The rules

all: objdir $(MAINPROGS)
debug: objdir $(MAINPROGS)
release: objdir $(MAINPROGS)
profile: objdir $(MAINPROGS)

.SECONDEXPANSION:
$(MAINPROGS): % : obj/%.o $(OBJFILES)
	$(CXX) $(LDFLAGS) -o $@ obj/$@.o $(OBJFILES) $(LIBS)

clean:
	rm -f $(MAINPROGS) source/*~ include/*~
	rm -rf obj

format:
	clang-format -i -style=file include/*.h source/*.cpp main/*.cpp

install:
	mkdir -p $(bindir)
	@list='$(MAINPROGS)'; \
	for prog in $$list; do \
	  echo $(INSTALL_PROG) $$prog $(bindir)/$$prog; \
	  $(INSTALL_PROG) $$prog $(bindir)/$$prog; \
	done
	mkdir -p $(datadir)/smartmet/dictionaries
	$(INSTALL_DATA) cnf/en.conf $(datadir)/smartmet/dictionaries
	mkdir -p $(datadir)/smartmet/formats
	$(INSTALL_DATA) cnf/netcdf.conf $(datadir)/smartmet/formats/
	$(INSTALL_DATA) cnf/grib.conf $(datadir)/smartmet/formats/
	$(INSTALL_DATA) cnf/bufr.conf $(datadir)/smartmet/formats/
	$(INSTALL_DATA) cnf/stations.csv $(datadir)/smartmet/
	$(INSTALL_DATA) cnf/parameters.csv $(datadir)/smartmet/

test:
	cd test && make test

objdir:
	@mkdir -p $(objdir)

rpm: clean $(SPEC).spec
	rm -f $(SPEC).tar.gz # Clean a possible leftover from previous attempt
	tar -czvf $(SPEC).tar.gz --exclude test --exclude-vcs --transform "s,^,$(SPEC)/," *
	rpmbuild -ta $(SPEC).tar.gz
	rm -f $(SPEC).tar.gz

.SUFFIXES: $(SUFFIXES) .cpp

obj/%.o : %.cpp
	$(CXX) $(CFLAGS) $(INCLUDES) -c -o $@ $<

-include obj/*.d
