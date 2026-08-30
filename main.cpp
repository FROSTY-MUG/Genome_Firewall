#include <iostream>
#include <string>
#include <vector>
#include <chrono>
#include <iomanip>
#include "AMRTrie.h"
#include "AMRDatabase.h"
#include "FastaMmapParser.h"
#include "ScoringEngine.h"
#include "PredictorEngine.h"
#include "REPLShell.h"

void printBanner() {
    std::cout << "========================================================================\n";
    std::cout << "               GENOME FIREWALL v2.0 - BIOSECURITY CLI                   \n";
    std::cout << "       Predictive Antimicrobial Resistance Decision Engine              \n";
    std::cout << "========================================================================\n";
}

void printUsage(const char* progName) {
    std::cerr << "Usage: " << progName << " --input <genome.fasta> --db <markers.csv> [options]\n";
    std::cerr << "       " << progName << " --interactive\n";
    std::cerr << "Options:\n";
    std::cerr << "  --input <file>        Input FASTA genome sequence file\n";
    std::cerr << "  --db <file>           AMR reference database CSV/TSV file\n";
    std::cerr << "  --fuzzy               Enable mutation-tolerant Hamming Distance <= 1 matching (default)\n";
    std::cerr << "  --exact               Force strict exact matching only (Hamming = 0)\n";
    std::cerr << "  --async               Enable concurrent multithreaded chunk processing (default)\n";
    std::cerr << "  --serial              Run single-threaded sequential scoring\n";
    std::cerr << "  --confidence <val>    Set prediction confidence threshold percentage (default: 75.0)\n";
    std::cerr << "  --interactive         Launch interactive REPL terminal session\n\n";
}

int main(int argc, char* argv[]) {
    std::string fastaFile = "sample.fasta";
    std::string amrDbFile = "markers.csv";
    bool enableFuzzy = true;
    bool enableAsync = true;
    bool interactiveMode = false;
    double confidenceThreshold = 75.0;

    for (int i = 1; i < argc; ++i) {
        std::string arg = argv[i];
        if (arg == "--interactive" || arg == "-i") {
            interactiveMode = true;
        } else if ((arg == "--input" || arg == "-in") && i + 1 < argc) {
            fastaFile = argv[++i];
        } else if ((arg == "--db" || arg == "-d") && i + 1 < argc) {
            amrDbFile = argv[++i];
        } else if (arg == "--exact") {
            enableFuzzy = false;
        } else if (arg == "--fuzzy") {
            enableFuzzy = true;
        } else if (arg == "--serial") {
            enableAsync = false;
        } else if (arg == "--async") {
            enableAsync = true;
        } else if (arg == "--confidence" && i + 1 < argc) {
            confidenceThreshold = std::stod(argv[++i]);
        } else if (i == 1 && arg[0] != '-') {
            fastaFile = arg;
        } else if (i == 2 && arg[0] != '-') {
            amrDbFile = arg;
        }
    }

    if (interactiveMode) {
        REPLShell repl;
        repl.run();
        return 0;
    }

    printBanner();
    std::cout << "[*] System Mode: " << (enableFuzzy ? "Mutation-Tolerant (Hamming <= 1)" : "Exact Match Only (Hamming = 0)")
              << " | " << (enableAsync ? "Multithreaded (std::async)" : "Sequential (Single-threaded)") << "\n";
    std::cout << "[*] Confidence Threshold: " << confidenceThreshold << "%\n\n";

    auto totalStart = std::chrono::high_resolution_clock::now();

    try {
        // 1. Initialize Trie & Database
        AMRTrie trie;
        AMRDatabase db;

        std::cout << "[*] Loading AMR Reference Catalog from: " << amrDbFile << "...\n";
        auto dbStart = std::chrono::high_resolution_clock::now();
        db.loadFromFile(amrDbFile, trie);
        auto dbEnd = std::chrono::high_resolution_clock::now();
        double dbTimeMs = std::chrono::duration<double, std::milli>(dbEnd - dbStart).count();

        std::cout << "    -> Indexed " << db.getMarkerCount() << " AMR signatures in 4-ary Prefix Trie ("
                  << trie.getNodeCount() << " nodes, Arena: " << (trie.getMemoryUsage() / 1024) << " KB)\n";
        std::cout << "    -> Fast-Rejection Bloom Filter initialized (" << trie.getBloomFilter().getBitSize() << " bits)\n";
        std::cout << "    -> Load & Index Time: " << std::fixed << std::setprecision(3) << dbTimeMs << " ms\n\n";

        // 2. Initialize Zero-Copy FASTA Parser & Scoring Engine
        std::cout << "[*] Initializing Zero-Copy Sequence Ingestion for: " << fastaFile << "...\n";
        FastaMmapParser parser(fastaFile);
        ScoringEngine engine(trie, db, enableFuzzy, confidenceThreshold);

        const size_t KMER_LENGTH = 21;
        const size_t CHUNK_SIZE = 4096;
        const size_t OVERLAP_SIZE = KMER_LENGTH - 1;

        std::string chunk;
        size_t totalBytesParsed = 0;
        size_t chunkCount = 0;

        auto scanStart = std::chrono::high_resolution_clock::now();

        while (parser.getNextChunk(chunk, CHUNK_SIZE, OVERLAP_SIZE)) {
            chunkCount++;
            totalBytesParsed += chunk.size();

            if (enableAsync) {
                engine.processSequenceChunkAsync(chunk, KMER_LENGTH);
            } else {
                engine.processSequenceChunk(chunk, KMER_LENGTH);
            }
        }

        if (enableAsync) {
            engine.waitForAsyncTasks();
        }

        auto scanEnd = std::chrono::high_resolution_clock::now();
        double scanTimeMs = std::chrono::duration<double, std::milli>(scanEnd - scanStart).count();

        std::cout << "[+] Scan completed across " << chunkCount << " chunks (" 
                  << (totalBytesParsed / 1024) << " KB scanned).\n";

        // 3. Extract Binary Feature Vector X & Evaluate Logistic Regression Predictions
        std::vector<int> X = engine.getBinaryFeatureVector();
        auto predictions = engine.getPredictor().evaluatePredictionsWithModel(db, X);
        std::string report = engine.getPredictor().generateASCIIReport(predictions, scanTimeMs, totalBytesParsed);
        std::cout << report;

        auto totalEnd = std::chrono::high_resolution_clock::now();
        double totalTimeMs = std::chrono::duration<double, std::milli>(totalEnd - totalStart).count();
        std::cout << "[*] Total Execution Wall-Clock Time: " << std::fixed << std::setprecision(2) << totalTimeMs << " ms\n\n";

    } catch (const std::exception& e) {
        std::cerr << "\n[!] FATAL ERROR: " << e.what() << "\n";
        return 1;
    }

    return 0;
}
