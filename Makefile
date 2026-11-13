CXX      = g++
# C++14 is used for compatibility with MinGW.org GCC 6.3 and modern GCC/Clang.
# -O3 enables aggressive loop unrolling, -march=native enables hardware SIMD vectorization, -flto enables cross-unit inlining.
CXXFLAGS = -std=c++14 -O3 -pthread -march=native -fomit-frame-pointer -flto \
           -Wall -Wextra \
           -Wno-unused-result -Wno-unused-parameter \
           -Wno-missing-field-initializers \
           -Wno-shift-count-overflow \
           -Iinclude

# MinGW uses -mthreads (native Win32 threading); Linux/macOS/Android uses -pthread
ifeq ($(OS),Windows_NT)
LDFLAGS  = -mthreads -flto
else
LDFLAGS  = -flto
endif

SRCDIR   = src
INCDIR   = include
OBJDIR   = obj
BINDIR   = .

SOURCES  = $(SRCDIR)/AMRDatabase.cpp \
           $(SRCDIR)/FastaMmapParser.cpp \
           $(SRCDIR)/AMRBloomFilter.cpp \
           $(SRCDIR)/TrieArena.cpp \
           $(SRCDIR)/AMRTrie.cpp \
           $(SRCDIR)/ProteomeTranslator.cpp \
           $(SRCDIR)/LogisticRegressor.cpp \
           $(SRCDIR)/PredictorEngine.cpp \
           $(SRCDIR)/ScoringEngine.cpp \
           $(SRCDIR)/REPLShell.cpp \
           $(SRCDIR)/main.cpp

OBJECTS  = $(patsubst $(SRCDIR)/%.cpp,$(OBJDIR)/%.o,$(SOURCES))
TARGET   = genome_firewall

all: $(TARGET)

$(TARGET): $(OBJECTS)
	$(CXX) $(OBJECTS) $(LDFLAGS) -o $@

$(OBJDIR)/%.o: $(SRCDIR)/%.cpp | $(OBJDIR)
	$(CXX) $(CXXFLAGS) -c $< -o $@

ifeq ($(OS),Windows_NT)
$(OBJDIR):
	-mkdir $(OBJDIR) 2>NUL

clean:
	-cmd /c "if exist $(OBJDIR)\*.o del /Q $(OBJDIR)\*.o" 2>NUL
	-cmd /c "if exist $(TARGET).exe del /Q $(TARGET).exe" 2>NUL
else
$(OBJDIR):
	mkdir -p $(OBJDIR)

clean:
	rm -f $(OBJDIR)/*.o $(OBJDIR)/*.obj $(TARGET) $(TARGET).exe
endif

test: $(TARGET)
	@echo [*] Running Verification Test on CARD Top Markers and Sample Genome...
	$(TARGET) --input data/sample_real.fasta --db data/markers_card_top.csv

serve: $(TARGET)
	@echo [*] Starting the local web console on http://127.0.0.1:8787
	@node api/server.js

.PHONY: all clean test serve
