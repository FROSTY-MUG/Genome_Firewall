#ifndef REPL_SHELL_H
#define REPL_SHELL_H

#include <string>
#include "AMRTrie.h"
#include "AMRDatabase.h"
#include "ScoringEngine.h"

class REPLShell {
public:
    REPLShell();
    ~REPLShell() = default;

    void run();

private:
    AMRTrie trie;
    AMRDatabase db;
    bool fuzzyEnabled{true};
    bool asyncEnabled{true};
    double confidenceThreshold{75.0};
    std::string currentDbFile{"markers.csv"};

    void printWelcomeBanner();
    void printHelp();
    void executeScan(const std::string& fastaFile);
    void printStatus();
};

#endif // REPL_SHELL_H
