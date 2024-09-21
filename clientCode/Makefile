# Compiler and flags
CXX = g++
CXXFLAGS = -pthread

# Target and source files
TARGET = client
SRCS = client.cpp clientUtils.cpp
OBJS = $(SRCS:.cpp=.o)

# Default target
all: $(TARGET)

# Build the target
$(TARGET): $(OBJS)
	$(CXX) -o $(TARGET) $(OBJS) $(CXXFLAGS)

# Compile source files into object files
%.o: %.cpp
	$(CXX) -c $< $(CXXFLAGS)

# Clean up object files and executable
clean:
	rm -f $(OBJS) $(TARGET)

# Phony target to avoid filename conflicts
.PHONY: all clean

