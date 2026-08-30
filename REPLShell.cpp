#include "REPLShell.h"
#include "FastaMmapParser.h"
#include <iostream>
#include <sstream>
#include <chrono>
#include <iomanip>

REPLShell::REPLShell() {
    db.loadFromFile(currentDbFile, trie);
}

void REPLShell::printWelcomeBanner() {
    std::cout << "========================================================================\n";
    std::cout << "               GENOME FIREWALL v2.0 - INTERACTIVE REPL SHELL            \n";
    std::cout << "         High-Performance Biosecurity & AMR Decision Engine             \n";
    std::cout << "========================================================================\n";
    std::cout << "Type 'help' for commands list or 'exit' to quit.\n\n";
}

void REPLShell::printHelp() {
    std::cout << "\nAvailable Commands:\n";
    std::cout << "  load_db <filepath>     : Dynamic loading/swapping of AMR Database CSV/TSV\n";
    std::cout << "  scan <fasta_file>      : Execute full Zero-Copy & Target-Gated scan on genome\n";
    std::cout << "  set_fuzzy <on|off>     : Toggle mutation-tolerant Hamming Distance = 1 matching\n";
    std::cout << "  set_async <on|off>     : Toggle multithreaded concurrent scoring\n";
    std::cout << "  set_conf <value>       : Adjust confidence threshold percentage (e.g. 75.0)\n";
    std::cout << "  status                 : Show current system state, Trie memory, & Bloom filter size\n";
    std::cout << "  help                   : Display this help message\n";
    std::cout << "  exit / quit            : Terminate interactive session\n\n";
}

void REPLShell::printStatus() {
    std::cout << "\n--- System Telemetry & Configuration Status ---\n";
    std::cout << "  Current DB File      : " << currentDbFile << "\n";
    std::cout << "  Indexed Signatures   : " << db.getMarkerCount() << "\n";
    std::cout << "  Trie Nodes Allocated : " << trie.getNodeCount() << " (Arena: " << (trie.getMemoryUsage() / 1024) << " KB)\n";
    std::cout << "  Bloom Filter BitSize : " << trie.getBloomFilter().getBitSize() << " bits\n";
    std::cout << "  Fuzzy Matching       : " << (fuzzyEnabled ? "ENABLED (Hamming <= 1)" : "DISABLED (Exact)") << "\n";
    std::cout << "  Multithreading       : " << (asyncEnabled ? "ENABLED (std::async)" : "DISABLED (Serial)") << "\n";
    std::cout << "  Confidence Threshold : " << confidenceThreshold << "%\n\n";
}

void REPLShell::executeScan(const std::string& fastaFile) {
    std::cout << "\n[*] Starting Genome Firewall Scan on: " << fastaFile << "...\n";
    try {
        FastaMmapParser parser(fastaFile);
        ScoringEngine engine(trie, db, fuzzyEnabled, confidenceThreshold);

        auto start = std::chrono::high_resolution_clock::now();
        std::string chunk;
        size_t totalBytes = 0;
        size_t chunks = 0;

        while (parser.getNextChunk(chunk, 4096, 20)) {
            chunks++;
            totalBytes += chunk.size();
            if (asyncEnabled) {
                engine.processSequenceChunkAsync(chunk, 21);
            } else {
                engine.processSequenceChunk(chunk, 21);
            }
        }
        (void)chunks;

        if (asyncEnabled) {
            engine.waitForAsyncTasks();
        }

        auto end = std::chrono::high_resolution_clock::now();
        double elapsedMs = std::chrono::duration<double, std::milli>(end - start).count();

        std::vector<int> X = engine.getBinaryFeatureVector();
        auto predictions = engine.getPredictor().evaluatePredictionsWithModel(db, X);
        std::string report = engine.getPredictor().generateASCIIReport(predictions, elapsedMs, totalBytes);
        std::cout << report;

    } catch (const std::exception& e) {
        std::cerr << "[!] Error executing scan: " << e.what() << "\n";
    }
}

void REPLShell::run() {
    printWelcomeBanner();
    std::string line;

    while (true) {
        std::cout << "genome_firewall> " << std::flush;
        if (!std::getline(std::cin, line)) break;

        std::stringstream ss(line);
        std::string cmd;
        ss >> cmd;

        if (cmd == "exit" || cmd == "quit") {
            std::cout << "Exiting Genome Firewall interactive shell. Goodbye!\n";
            break;
        } else if (cmd == "help") {
            printHelp();
        } else if (cmd == "status") {
            printStatus();
        } else if (cmd == "load_db") {
            std::string path;
            if (ss >> path) {
                currentDbFile = path;
                trie.clear();
                db.loadFromFile(path, trie);
            } else {
                std::cout << "Usage: load_db <csv_file>\n";
            }
        } else if (cmd == "scan") {
            std::string path;
            if (ss >> path) {
                executeScan(path);
            } else {
                std::cout << "Usage: scan <fasta_file>\n";
            }
        } else if (cmd == "set_fuzzy") {
            std::string opt;
            if (ss >> opt) {
                fuzzyEnabled = (opt == "on" || opt == "1" || opt == "true");
                std::cout << "Fuzzy matching set to: " << (fuzzyEnabled ? "ON" : "OFF") << "\n";
            }
        } else if (cmd == "set_async") {
            std::string opt;
            if (ss >> opt) {
                asyncEnabled = (opt == "on" || opt == "1" || opt == "true");
                std::cout << "Multithreading set to: " << (asyncEnabled ? "ON" : "OFF") << "\n";
            }
        } else if (cmd == "set_conf") {
            double val;
            if (ss >> val) {
                confidenceThreshold = val;
                std::cout << "Confidence threshold set to: " << confidenceThreshold << "%\n";
            }
        } else if (!cmd.empty()) {
            std::cout << "Unknown command: '" << cmd << "'. Type 'help' for available commands.\n";
        }
    }
}
