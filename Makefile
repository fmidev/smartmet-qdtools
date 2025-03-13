MODULE = qdtools
SPEC = smartmet-qdtools

REQUIRES = gdal fmt

include $(shell echo $${PREFIX-/usr})/share/smartmet/devel/makefile.inc

# mdsplib does not declare things correctly

MAINFLAGS += -fpermissive


# Default compiler flags

DEFINES = -DUNIX

# gdal 32 from pgdg

INCLUDES +=  \
	-isystem $(includedir)/netcdf-3 \
	-isystem $(includedir)/bufr \
	-isystem $(includedir)/libecbufr \
	-isystem $(includedir)/ecbufr \
	-I$(includedir)/smartmet

LIBS += $(PREFIX_LDFLAGS) \
     $(EXTRA_LIBS) \
	-lsmartmet-calculator \
	-lsmartmet-smarttools \
	-lsmartmet-newbase \
	-lsmartmet-macgyver \
	-lsmartmet-gis \
	-lsmartmet-imagine \
	-lboost_regex \
	-lboost_program_options \
	-lboost_iostreams \
	-lboost_thread \
	-lboost_system \
	$(REQUIRED_LIBS) \
	-lbz2 -ljpeg -lpng -lz -lrt \
	-lpthread

# Each part really needs only part of libraries used below.
# Link them with only required
EXTRA_LIBS :=
bufrtoqd: EXTRA_LIBS += -lecbufr
radartoqd: EXTRA_LIBS += -lecbufr -lbufr
h5toqd: EXTRA_LIBS += -lMXADataModel -lhdf5
metar2qd: EXTRA_LIBS += -lmetar
grib2tojpg grib2toqd gribtoqd qdtogrib: EXTRA_LIBS += -leccodes
laps2qd nc2qd nctoqd wrftoqd: EXTRA_LIBS += -lnetcdf_c++ -lnetcdf

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
$(MAINPROGS): % : obj/%.o obj/libqdtools.a
	$(CXX) $(LDFLAGS) -o $@ obj/$@.o -Lobj -lqdtools $(LIBS)

obj/libqdtools.a: $(OBJFILES)
	rm -f $@
	ar rcs $@ $(OBJFILES)
	ranlib $@

clean:
	rm -f $(MAINPROGS) source/*~ include/*~
	rm -rf obj
	$(MAKE) -C test $@

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
	$(CXX) $(CFLAGS) $(INCLUDES) -c -MD -MF $(patsubst obj/%.o, obj/%.d, $@) -MT $@ -o $@ $<

-include obj/*.d
