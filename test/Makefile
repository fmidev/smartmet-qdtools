PROG = $(wildcard *.test)

all: clean $(PROG)
test: clean $(PROG)
.PHONY: $(PROG)

test:
	@echo Running tests:
	@rm -f *.err
	@for prog in $(PROG); do \
	  ( echo; echo $$prog; ./$$prog || touch $$prog.err ) ; \
	done
	@test `find . -name \*.err | wc -l` = "0" || ( echo ; echo "The following tests have errors:" ; \
		for i in *.err ; do echo `basename $$i .err`; done ; rm -f *.err ; false )

clean:
	rm -f results/*.tmp *~ results/gribtoqd/*.tmp*

