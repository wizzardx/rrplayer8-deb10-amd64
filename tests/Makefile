TESTS = *.h

all: run_tests

# Rules to build your targets

# A rule that runs the unit tests
run_tests: runner
	./runner

# How to build the test runner
runner: runner.cpp
	g++ -o $@ $^

# How to generate the test runner
runner.cpp: $(TESTS)
	cxxtestgen.pl -o $@ --error-printer $^


