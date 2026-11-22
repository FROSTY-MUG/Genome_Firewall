#ifndef REPL_SHELL_H
#define REPL_SHELL_H

#include <string>
#include <vector>
#include "AMRTrie.h"
#include "AMRDatabase.h"
#include "ScoringEngine.h"
#include "PredictorEngine.h"

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
    std::string currentDbFile{"markers_large.csv"};

    // Cached telemetry from the most recent scan
    bool hasLastScan{false};
    std::string lastScanTarget;
    bool lastScanWasFile{true};
    size_t lastScanBytes{0};
    double lastScanTimeMs{0.0};
    int lastExactHits{0};
    int lastMutatedHits{0};
    int lastResistanceScore{0};
    std::vector<int> lastFeatureVector;
    std::vector<DrugPrediction> lastPredictions;

    void printWelcomeBanner();
    void printMenu();
    void printHelp();
    void printStatus();

    // Interactive Terminal Features
    void executeScan(const std::string& fastaFile);
    void executeSequenceScan(const std::string& rawSeq);
    void executeTrieQuery(const std::string& kmer);
    void executeMmapDiagnostics(const std::string& fastaFile);
    void printScoresAndFeatures();
    void configureFeatures();
};

#endif // REPL_SHELL_H
