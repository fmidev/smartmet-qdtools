PROG = $(patsubst %.test,%,$(wildcard *.test))
TEST = $(PROG:%=%.test)

all: $(PROG)
test: $(PROG)
.PHONY: $(PROG)

$(PROG):
	./$@.test

clean:
	rm -f results/*.tmp *~
