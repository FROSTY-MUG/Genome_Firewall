CXX      = g++
CC       = gcc
CXXFLAGS = -std=c++14 -O3 -pthread -march=native -fomit-frame-pointer -flto \
           -Wall -Wextra -Wno-unused-result -Wno-unused-parameter \
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
TARGET   = genome_firewall

SOURCES  = $(wildcard $(SRCDIR)/*.cpp)
CSOURCES = $(wildcard $(SRCDIR)/*.c)
OBJECTS  = $(patsubst $(SRCDIR)/%.cpp, $(OBJDIR)/%.o, $(SOURCES)) \
           $(patsubst $(SRCDIR)/%.c, $(OBJDIR)/%.o, $(CSOURCES))

all: $(TARGET)

$(TARGET): $(OBJECTS)
	$(CXX) $(OBJECTS) $(LDFLAGS) -o $@

$(OBJDIR)/%.o: $(SRCDIR)/%.cpp | $(OBJDIR)
	$(CXX) $(CXXFLAGS) -c $< -o $@

$(OBJDIR)/%.o: $(SRCDIR)/%.c | $(OBJDIR)
	$(CC) $(CFLAGS) -c $< -o $@

$(OBJDIR):
	@mkdir -p $(OBJDIR) 2>/dev/null || cmd /c "if not exist $(OBJDIR) mkdir $(OBJDIR)" 2>nul || true

clean:
	@rm -f $(OBJDIR)/*.o $(OBJDIR)/*.obj $(TARGET).exe $(TARGET) test_suite* fasta_benchmark* card_extractor* 2>/dev/null || cmd /c "if exist $(OBJDIR)\*.o del /Q $(OBJDIR)\*.o & if exist $(TARGET).exe del /Q $(TARGET).exe" 2>nul || true

test: $(TARGET)
	@echo [*] Running Verification Test on CARD Top Markers and Sample Genome...
	./$(TARGET) --input data/sample_real.fasta --db data/markers_card_top.csv

unit-test: tests/test_genome_firewall.cpp $(filter-out $(OBJDIR)/main.o $(OBJDIR)/REPLShell.o, $(OBJECTS))
	$(CXX) $(CXXFLAGS) tests/test_genome_firewall.cpp $(filter-out $(OBJDIR)/main.o $(OBJDIR)/REPLShell.o, $(OBJECTS)) $(LDFLAGS) -o test_suite
	./test_suite
	@rm -f test_suite.exe test_suite 2>/dev/null || true

benchmark: tools/fasta_benchmark.cpp $(filter-out $(OBJDIR)/main.o $(OBJDIR)/REPLShell.o, $(OBJECTS))
	$(CXX) $(CXXFLAGS) tools/fasta_benchmark.cpp $(filter-out $(OBJDIR)/main.o $(OBJDIR)/REPLShell.o, $(OBJECTS)) $(LDFLAGS) -o fasta_benchmark
	./fasta_benchmark
	@rm -f fasta_benchmark.exe fasta_benchmark 2>/dev/null || true

extract-tool: tools/card_dataset_extractor.cpp
	$(CXX) $(CXXFLAGS) tools/card_dataset_extractor.cpp -o card_extractor
	./card_extractor
	@rm -f card_extractor.exe card_extractor 2>/dev/null || true

serve: $(TARGET)
	@echo [*] Starting the local web console on http://127.0.0.1:8787
	@node api/server.js

.PHONY: all clean test unit-test benchmark extract-tool serve
