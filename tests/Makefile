TESTS = test_cxxtest.h

all: run_tests

# Rules to build your targets

# A rule that runs the unit tests
run_tests: tests
	./tests

# How to build the test runner
tests: tests.cpp
	g++ -o $@ $^

# How to generate the test runner
tests.cpp: $(TESTS)
	cxxtestgen.pl -o $@ --error-printer $^


