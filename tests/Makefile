CXX      = g++
CXXFLAGS = -std=c++17 -Wall -Wextra -Wpedantic -I./mocks
TARGET   = test_runner

all: $(TARGET)
	./$(TARGET)

$(TARGET): test_main.cpp mocks/Arduino.h mocks/CircularBuffer.h \
           mocks/MD_MAX72xx.h mocks/MD_Parola.h mocks/SPI.h \
           ../GearShift6_8x8.ino
	$(CXX) $(CXXFLAGS) -o $(TARGET) test_main.cpp

clean:
	rm -f $(TARGET)

.PHONY: all clean
