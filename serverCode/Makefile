# Compiler and flags
CXX = g++
CXXFLAGS = -pthread
LDFLAGS = -lssl -lcrypto

# Target and source files
TARGET = server
SRCS = server.cpp serverUtils.cpp
OBJS = $(SRCS:.cpp=.o)

# Default target
all: $(TARGET)

# Build the target
$(TARGET): $(OBJS)
	$(CXX) -o $(TARGET) $(OBJS) $(CXXFLAGS) $(LDFLAGS)

# Compile source files into object files
%.o: %.cpp
	$(CXX) -c $< $(CXXFLAGS)

# Clean up object files and executable
clean:
	rm -f $(OBJS) $(TARGET)

# Phony target to avoid filename conflicts
.PHONY: all clean

