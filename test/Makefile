PROG = $(patsubst %.test,%,$(wildcard *.test))
TEST = $(PROG:%=%.test)

all: clean $(PROG)
test: clean $(PROG)
.PHONY: $(PROG)

$(PROG): % : ../%
	./$@.test

clean:
	rm -f results/*.tmp *~ results/gribtoqd/*.tmp*

