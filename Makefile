# Compiler to use
CXX = g++
CXXFLAGS = -std=c++11
TARGET = demo
SRC = demo.cpp

$(TARGET): $(SRC)
	$(CXX) $(CXXFLAGS) $(SRC) -o $(TARGET)

clean:
	rm -f $(TARGET)

