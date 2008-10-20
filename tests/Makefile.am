INCLUDES = 
METASOURCES = AUTO
noinst_PROGRAMS = tests
tests_SOURCES = tests.cpp

TESTS = test_cxxtest.h
tests.cpp: $(TESTS)
	cxxtestgen.pl -o $@ --error-printer $^

