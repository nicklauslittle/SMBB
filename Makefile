
# Required variables
PROJECT_NAME :=smbb
DEFINE_ARGS :=-D_FILE_OFFSET_BITS=64 -D_LARGE_FILES=1 -D_WIN32_WINNT=0x0600
LDLIBS :=-lrt -ldl
DIR :=.

# Optional variables
CPPFLAGS +=$(DEFINE_ARGS)
CXXFLAGS :=-Wall -pedantic -O2

SRC :=$(wildcard $(DIR)/src/smbb/*.cxx $(DIR)/src/smbb/*/*.cxx)
OBJ :=$(patsubst $(DIR)/src/smbb/%.cxx,obj/%.o,$(SRC))
TESTS :=$(patsubst $(DIR)/test/%.cxx,%.exe,$(wildcard $(DIR)/test/*.cxx))
RUN_TESTS :=$(patsubst %.exe,%.exe.run,$(TESTS))

all: lib$(PROJECT_NAME).a

tests: $(TESTS)

check: $(RUN_TESTS)

clean:
	rm -rf obj
	rm -rf lib$(PROJECT_NAME).a
	rm -rf $(TESTS)

.PHONY: all tests check clean

lib$(PROJECT_NAME).a: $(OBJ)
	$(AR) $(ARFLAGS) $@ $^

obj/%.o: $(DIR)/src/smbb/%.cxx
	mkdir -p $(@D)
	$(CXX) $(CPPFLAGS) $(CXXFLAGS) -c $< -o $@

# Tests
%.exe: $(DIR)/test/%.cxx
	$(CXX) $(CPPFLAGS) $(CXXFLAGS) -I$(DIR)/test/catch2 -I$(DIR)/src -o $@ $^ $(LDLIBS)

%.exe.run: %.exe
	./$<
