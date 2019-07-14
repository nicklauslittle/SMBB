
PROJECT_NAME :=smbb
COMPILE_ARGS :=-D_FILE_OFFSET_BITS=64 -D_LARGE_FILES=1
LINK_ARGS :=-lrt -ldl
DIR :=.

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
	$(CXX) $(CPPFLAGS) $(CXXFLAGS) $(COMPILE_ARGS) -Wall -pedantic -c $< -o $@

# Tests
%.exe: $(DIR)/test/%.cxx lib$(PROJECT_NAME).a
	$(CXX) $(CPPFLAGS) $(CXXFLAGS) $(COMPILE_ARGS) -I$(DIR)/test/catch2 -I$(DIR)/src -Wall -pedantic -o $@ $^ $(LINK_ARGS)

%.exe.run: %.exe
	./$<
