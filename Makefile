CXX = g++
CXXFLAGS = -std=c++17 -O3 -pthread -Wall -Wextra
LDFLAGS = -pthread

SRC_DIR = .
OBJ_DIR = .

SOURCES = AMRDatabase.cpp FastaMmapParser.cpp AMRBloomFilter.cpp TrieArena.cpp AMRTrie.cpp LogisticRegressor.cpp PredictorEngine.cpp ScoringEngine.cpp REPLShell.cpp main.cpp
OBJECTS = $(SOURCES:.cpp=.o)
TARGET = genome_firewall

all: $(TARGET)

$(TARGET): $(OBJECTS)
	$(CXX) $(OBJECTS) $(LDFLAGS) -o $@

%.o: %.cpp
	$(CXX) $(CXXFLAGS) -c $< -o $@

clean:
	rm -f *.o $(TARGET) $(TARGET).exe

.PHONY: all clean
