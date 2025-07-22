CXX = g++
CXXFLAGS = -std=c++17 -Iinclude -Wall -g

SRC := $(wildcard src/*.cpp) $(wildcard test/*.cpp)
OBJ := $(patsubst %.cpp, build/%.o, $(notdir $(SRC)))
TARGET := build/test

build/%.o: src/%.cpp
	mkdir -p build
	$(CXX) $(CXXFLAGS) -c $< -o $@

build/%.o: test/%.cpp
	mkdir -p build
	$(CXX) $(CXXFLAGS) -c $< -o $@

$(TARGET): $(OBJ)
	$(CXX) $(CXXFLAGS) $(OBJ) -o $(TARGET)

.PHONY: clean
clean:
	rm -rf build/*