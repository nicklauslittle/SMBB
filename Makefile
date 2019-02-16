
PROJECT_NAME :=smbb
COMPILE_ARGS :=-D_FILE_OFFSET_BITS=64 -D_LARGE_FILES=1
SRC :=$(wildcard src/smbb/*.cpp)
TESTS :=$(patsubst src/%.cpp,%.exe,$(wildcard test/*.cpp))
RUN_TESTS :=$(patsubst %.exe,%.exe.run,$(TESTS))

all: $(PROJECT_NAME).a

tests: $(TESTS)

check: $(RUN_TESTS)

clean:
	rm -rf obj
	rm -rf $(PROJECT_NAME).a
	rm -rf $(TESTS)

.PHONY: all tests check $(RUN_TESTS) clean

$(PROJECT_NAME).a: $(patsubst src/%.cpp,obj/%.o,$(SRC))
	$(AR) $(ARFLAGS) $@ $^

obj/%.o: src/%.cpp
	mkdir -p $(@D)
	$(CXX) $(CPPFLAGS) $(CXXFLAGS) $(COMPILE_ARGS) -Isrc -Wall -pedantic -c $< -o $@

# Tests
%.exe: test/%.cpp $(PROJECT_NAME).a
	$(CXX) $(CPPFLAGS) $(CXXFLAGS) $(COMPILE_ARGS) -Itest/catch2 -Isrc -Wall -pedantic $^ -o $@ -lrt -ldl

%.exe.run: %.exe
	$<
