CXX      = g++
# C++14 is used instead of C++17 for compatibility with MinGW.org GCC 6.3.
# -pthread enables std::atomic. Native OS threads (Win32/POSIX) are used
# in ScoringEngine instead of the broken <future>/<mutex> in MinGW.org GCC 6.3.
CXXFLAGS = -std=c++14 -O2 -pthread \
           -Wall -Wextra \
           -Wno-unused-result -Wno-unused-parameter \
           -Wno-missing-field-initializers \
           -Wno-shift-count-overflow

# MinGW.org uses -mthreads (not -lpthread which doesn't exist in this toolchain)
LDFLAGS  = -mthreads

SOURCES = AMRDatabase.cpp FastaMmapParser.cpp AMRBloomFilter.cpp TrieArena.cpp \
          AMRTrie.cpp LogisticRegressor.cpp PredictorEngine.cpp \
          ScoringEngine.cpp REPLShell.cpp main.cpp
OBJECTS = $(SOURCES:.cpp=.o)
TARGET  = genome_firewall

all: $(TARGET)

$(TARGET): $(OBJECTS)
	$(CXX) $(OBJECTS) $(LDFLAGS) -o $@

%.o: %.cpp
	$(CXX) $(CXXFLAGS) -c $< -o $@

# Windows-compatible clean (tries del first, falls back to rm for Linux/macOS)
clean:
	-del /Q *.o 2>NUL
	-del /Q $(TARGET).exe 2>NUL
	-rm -f *.o $(TARGET)

.PHONY: all clean
