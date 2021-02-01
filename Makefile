MODULE = qdtools
SPEC = smartmet-qdtools

REQUIRES = gdal fmt

include $(shell echo $${PREFIX-/usr})/share/smartmet/devel/makefile.inc

# mdsplib does not declare things correctly

MAINFLAGS += -fpermissive


# Default compiler flags

DEFINES = -DUNIX -DWGS84

# gdal 32 from pgdg

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
	$(REQUIRED_LIBS) \
	-lmetar \
	-ljasper \
	-leccodes \
	-lnetcdf_c++ -lnetcdf \
	-lMXADataModel -lhdf5 \
	-lbufr \
	-lecbufr \
	-lbz2 -ljpeg -lpng -lz -lrt \
	-lpthread

# Compilation directories

vpath %.cpp source main
vpath %.h include
vpath %.o $(objdir)
vpath %.d $(objdir)

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
	rpmbuild -tb $(SPEC).tar.gz
	rm -f $(SPEC).tar.gz

.SUFFIXES: $(SUFFIXES) .cpp

obj/%.o : %.cpp
	$(CXX) $(CFLAGS) $(INCLUDES) -c -o $@ $<

-include obj/*.d
