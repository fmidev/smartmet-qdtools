PROG = $(wildcard *.test)

DATA_DIR := /usr/share/smartmet/test/data/qdtools/input
RESULT_DIR := /usr/share/smartmet/test/data/qdtools/output

all: clean $(PROG)
test: clean $(PROG)
.PHONY: $(PROG)

test:
	$(MAKE) test-data
	sh ./RunTests.sh

test-data:	results-dir data-dir

results-dir:
	rm -rf results/*
	mkdir -p results
	for result_file in $$(cd $(RESULT_DIR) && find . -type f | grep -v tmp ) ; do \
	    mkdir -p results/$$(dirname $$result_file); \
	    ln -srf $(RESULT_DIR)/$$result_file results/$$result_file; \
	done

data-dir:
	rm -rf data/*
	mkdir -p data
	for data_file in $$(cd $(DATA_DIR) && find . -type f -a -not -name '*.tmp') ; do \
	    mkdir -p data/$$(dirname $$data_file); \
	    ln -srf $(DATA_DIR)/$$data_file data/$$data_file; \
	done

clean:
	rm -rf results data *.err *.xfail *.xpass
