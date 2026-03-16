.PHONY: all doxy clean
CXX = g++
CXXFLAGS = -Wall
TARGET = lab1
SOURCES = t.cpp
OBJECTS = $(SOURCES:%.cpp=%.o)
DEPS = $(wildcard *.h)
all: $(TARGET)
doxy:
	doxygen
$(TARGET): $(OBJECTS)
	$(CXX) $(OBJECTS) $(CXXFLAGS) -o $(TARGET)
%.o: %.cpp $(DEPS)
	$(CXX) $(CXXFLAGS) -c $< -o $@
clean:
	rm -f $(OBJECTS) $(TARGET)
