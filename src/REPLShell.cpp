#include "REPLShell.h"
#include "FastaMmapParser.h"
#include <iostream>
#include <sstream>
#include <chrono>
#include <iomanip>
#include <algorithm>
#include <cctype>
#include <fstream>

REPLShell::REPLShell() {
    // Check if data/markers_card_top.csv exists, or data/markers_large.csv, or markers_large.csv
    if (std::ifstream("data/markers_card_top.csv").good()) {
        currentDbFile = "data/markers_card_top.csv";
    } else if (std::ifstream("data/markers_large.csv").good()) {
        currentDbFile = "data/markers_large.csv";
    } else if (std::ifstream("markers_large.csv").good()) {
        currentDbFile = "markers_large.csv";
    } else {
        currentDbFile = "data/markers.csv";
    }
    db.loadFromFile(currentDbFile, trie);
}

void REPLShell::printWelcomeBanner() {
    std::cout << "\n========================================================================\n";
    std::cout << "               GENOME FIREWALL v2.0 - DIRECT TERMINAL SYSTEM            \n";
    std::cout << "       Predictive Biosecurity Engine | Zero-Copy I/O | 4-ary Trie       \n";
    std::cout << "========================================================================\n";
    std::cout << "[*] Direct interactive terminal test system active.\n";
    std::cout << "[*] Database loaded: " << currentDbFile << " (" << db.getMarkerCount() << " AMR signatures indexed)\n";
    std::cout << "Type 'menu' to view options, or enter a command directly (e.g., 'scan', 'help').\n\n";
}

void REPLShell::printMenu() {
    std::cout << "\n+----------------------------------------------------------------------+\n";
    std::cout << "|                 DIRECT TERMINAL TEST SYSTEM MENU                     |\n";
    std::cout << "+----------------------------------------------------------------------+\n";
    std::cout << "  [1] Scan FASTA File (Zero-Copy Memory-Mapped Streaming)              \n";
    std::cout << "  [2] Enter / Paste Raw FASTA Sequence Directly                        \n";
    std::cout << "  [3] Query 4-ary K-mer Trie (Exact & Mutation Tolerance Test)         \n";
    std::cout << "  [4] Memory-Map & Zero-Copy I/O Diagnostics                           \n";
    std::cout << "  [5] View Test Scores & Binary Feature Vector X                       \n";
    std::cout << "  [6] Configure System Features (Fuzzy, Multithreading, Conf, DB)      \n";
    std::cout << "  [7] Quick Benchmark Demo (sample_real.fasta)                         \n";
    std::cout << "  [8] Show System Status & Architecture Telemetry                      \n";
    std::cout << "  [9] Exit Terminal Session                                            \n";
    std::cout << "+----------------------------------------------------------------------+\n\n";
}

void REPLShell::printHelp() {
    std::cout << "\nAvailable Terminal Commands:\n";
    std::cout << "  menu                   : Display the numbered interactive options menu\n";
    std::cout << "  scan [filepath]        : Run Zero-Copy memory-mapped scan on a FASTA file\n";
    std::cout << "  scan_seq [nucleotides] : Test a raw DNA sequence directly in the terminal\n";
    std::cout << "  query [kmer]           : Test a k-mer against Bloom filter & 4-ary Trie\n";
    std::cout << "  mmap [filepath]        : Inspect OS memory mapping & chunking telemetry\n";
    std::cout << "  scores / features      : Inspect last scan test scores & binary feature vector\n";
    std::cout << "  config                 : Enter interactive feature configuration wizard\n";
    std::cout << "  load_db <filepath>     : Dynamically load/swap AMR reference CSV catalog\n";
    std::cout << "  set_fuzzy <on|off>     : Toggle mutation tolerance (Hamming <= 1 matching)\n";
    std::cout << "  set_async <on|off>     : Toggle multithreading concurrency (native threads)\n";
    std::cout << "  set_conf <value>       : Set prediction confidence threshold (e.g. 75.0)\n";
    std::cout << "  status                 : Display current memory, trie, and feature status\n";
    std::cout << "  demo                   : Execute quick scan of sample_real.fasta demo genome\n";
    std::cout << "  help                   : Show this command guide\n";
    std::cout << "  exit / quit            : Terminate interactive terminal session\n\n";
}

void REPLShell::printStatus() {
    std::cout << "\n========================================================================\n";
    std::cout << "               GENOME FIREWALL ARCHITECTURE & TELEMETRY                 \n";
    std::cout << "========================================================================\n";
    std::cout << "  AMR Database File    : " << currentDbFile << "\n";
    std::cout << "  Total AMR Signatures : " << db.getMarkerCount() << " curated resistance genes\n";
    std::cout << "  4-ary Prefix Trie    : " << trie.getNodeCount() << " nodes allocated in Arena (" 
              << (trie.getMemoryUsage() / 1024) << " KB RAM)\n";
    std::cout << "  Alphabet Encoding    : 4-ary mapping {A:0, C:1, G:2, T:3}\n";
    std::cout << "  Bloom Filter Array   : " << trie.getBloomFilter().getBitSize() << " bits (1 MB array, 3 hashes)\n";
    std::cout << "  Mutation Tolerance   : " << (fuzzyEnabled ? "ENABLED (Hamming Distance <= 1 SNP)" : "DISABLED (Exact Only)") << "\n";
    std::cout << "  Execution Pipeline   : " << (asyncEnabled ? "CONCURRENT (Multithreaded chunks)" : "SEQUENTIAL (Single-threaded)") << "\n";
    std::cout << "  Decision Confidence  : " << confidenceThreshold << "% threshold\n";
    if (hasLastScan) {
        std::cout << "  Last Target Scanned  : " << lastScanTarget << " (" << (lastScanBytes / 1024) << " KB)\n";
        std::cout << "  Last Scan Time       : " << std::fixed << std::setprecision(2) << lastScanTimeMs << " ms\n";
        std::cout << "  Last Test Scores     : Exact: " << lastExactHits << " | Mutated: " << lastMutatedHits 
                  << " | Resistance Score: " << lastResistanceScore << "\n";
    }
    std::cout << "========================================================================\n\n";
}

void REPLShell::executeScan(const std::string& fastaFile) {
    std::string targetFile = fastaFile;
    if (targetFile.empty()) {
        std::string defFile = std::ifstream("data/sample_real.fasta").good() ? "data/sample_real.fasta" : "sample_real.fasta";
        std::cout << "Enter FASTA file path [default: " << defFile << "]: " << std::flush;
        std::getline(std::cin, targetFile);
        if (targetFile.empty()) targetFile = defFile;
    }

    // Verify file exists
    std::ifstream checkFile(targetFile);
    if (!checkFile.good()) {
        std::cerr << "[!] Error: Cannot open file '" << targetFile << "'. Please verify the path.\n";
        return;
    }
    checkFile.close();

    std::cout << "\n========================================================================\n";
    std::cout << "               INITIALIZING ZERO-COPY MEMORY-MAPPED SCAN                \n";
    std::cout << "========================================================================\n";
    std::cout << "[*] Target FASTA File : " << targetFile << "\n";
    std::cout << "[*] Reference Catalog : " << currentDbFile << " (" << db.getMarkerCount() << " markers)\n";
    std::cout << "[*] Mode              : " << (fuzzyEnabled ? "Mutation-Tolerant (Hamming <= 1)" : "Exact Match Only")
              << " | " << (asyncEnabled ? "Multithreaded (Native Threads)" : "Sequential") << "\n";

    try {
        FastaMmapParser parser(targetFile);
        ScoringEngine engine(trie, db, fuzzyEnabled, confidenceThreshold);

        std::cout << "[+] Memory-Map Status : " 
                  << (parser.isMemoryMapped() ? "Zero-Copy OS Memory-Mapped View Active" : "Fallback Stream Active")
                  << " (" << parser.getFileSize() << " bytes mapped)\n";

        const size_t KMER_LEN = 21;
        const size_t CHUNK_SZ = 4096;
        const size_t OVERLAP_SZ = KMER_LEN - 1;

        std::string chunk;
        size_t totalBytes = 0;
        size_t chunkCount = 0;

        auto scanStart = std::chrono::high_resolution_clock::now();

        while (parser.getNextChunk(chunk, CHUNK_SZ, OVERLAP_SZ)) {
            chunkCount++;
            totalBytes += chunk.size();
            if (asyncEnabled) {
                engine.processSequenceChunkAsync(chunk, KMER_LEN);
            } else {
                engine.processSequenceChunk(chunk, KMER_LEN);
            }
        }

        if (asyncEnabled) {
            engine.waitForAsyncTasks();
        }

        auto scanEnd = std::chrono::high_resolution_clock::now();
        double elapsedMs = std::chrono::duration<double, std::milli>(scanEnd - scanStart).count();

        // Binary Feature Vector X & Logistic Predictions
        std::vector<int> X = engine.getBinaryFeatureVector();
        auto predictions = engine.getPredictor().evaluatePredictionsWithModel(db, X);
        std::string report = engine.getPredictor().generateASCIIReport(predictions, elapsedMs, totalBytes);
        std::cout << report;

        // Print Test Scores breakdown
        std::cout << "\n+--- TEST SCORES & HIT TELEMETRY -----------------------------------+\n";
        std::cout << "  Exact Marker Hits    : " << engine.getExactHits() << " (100% nucleotide identity)\n";
        std::cout << "  Mutated Marker Hits  : " << engine.getMutatedHits() << " (Hamming Distance = 1 SNP match)\n";
        std::cout << "  Composite Score      : " << engine.getResistanceScore() << " resistance points\n";
        int activeFeatures = 0;
        for (int v : X) if (v > 0) activeFeatures++;
        std::cout << "  Features Triggered   : " << activeFeatures << " of " << X.size() 
                  << " AMR determinants active in Feature Vector X\n";
        std::cout << "+-------------------------------------------------------------------+\n\n";

        // Cache results
        hasLastScan = true;
        lastScanTarget = targetFile;
        lastScanWasFile = true;
        lastScanBytes = totalBytes;
        lastScanTimeMs = elapsedMs;
        lastExactHits = engine.getExactHits();
        lastMutatedHits = engine.getMutatedHits();
        lastResistanceScore = engine.getResistanceScore();
        lastFeatureVector = X;
        lastPredictions = predictions;

    } catch (const std::exception& e) {
        std::cerr << "[!] Error during scan: " << e.what() << "\n";
    }
}

void REPLShell::executeSequenceScan(const std::string& rawSeq) {
    std::string seq = rawSeq;
    if (seq.empty()) {
        std::cout << "\nEnter or paste raw nucleotide DNA sequence (multi-line or FASTA supported):\n";
        std::cout << "(Press Enter on an empty line when finished)\n";
        std::cout << "> " << std::flush;
        std::string line;
        while (std::getline(std::cin, line)) {
            if (line.empty()) break;
            // Ignore FASTA header lines
            if (line[0] == '>') continue;
            seq += line;
            std::cout << "> " << std::flush;
        }
    }

    // Clean sequence: remove whitespace, convert to uppercase
    std::string cleanSeq;
    cleanSeq.reserve(seq.size());
    for (char c : seq) {
        if (std::isspace(static_cast<unsigned char>(c))) continue;
        char up = std::toupper(static_cast<unsigned char>(c));
        if (up == 'A' || up == 'C' || up == 'G' || up == 'T') {
            cleanSeq.push_back(up);
        }
    }

    if (cleanSeq.length() < 21) {
        std::cerr << "[!] Sequence length (" << cleanSeq.length() 
                  << " bp) is shorter than 21-mer sliding window minimum (21 bp).\n";
        return;
    }

    std::cout << "\n========================================================================\n";
    std::cout << "            DIRECT NUCLEOTIDE SEQUENCE INFERENCE REPORT                 \n";
    std::cout << "========================================================================\n";
    std::cout << "[*] Sequence Length   : " << cleanSeq.length() << " base pairs (bp)\n";
    std::cout << "[*] 21-mer Windows    : " << (cleanSeq.length() - 21 + 1) << " sliding positions\n";
    std::cout << "[*] Mode              : " << (fuzzyEnabled ? "Mutation-Tolerant (Hamming <= 1)" : "Exact Match Only") << "\n";

    auto start = std::chrono::high_resolution_clock::now();
    ScoringEngine engine(trie, db, fuzzyEnabled, confidenceThreshold);
    engine.processSequenceChunk(cleanSeq, 21);

    auto end = std::chrono::high_resolution_clock::now();
    double elapsedMs = std::chrono::duration<double, std::milli>(end - start).count();

    std::vector<int> X = engine.getBinaryFeatureVector();
    auto predictions = engine.getPredictor().evaluatePredictionsWithModel(db, X);
    std::string report = engine.getPredictor().generateASCIIReport(predictions, elapsedMs, cleanSeq.length());
    std::cout << report;

    std::cout << "\n+--- TEST SCORES FOR DIRECT SEQUENCE -------------------------------+\n";
    std::cout << "  Exact Matches        : " << engine.getExactHits() << "\n";
    std::cout << "  Mutated Matches (SNP): " << engine.getMutatedHits() << "\n";
    std::cout << "  Resistance Score     : " << engine.getResistanceScore() << "\n";
    int activeFeatures = 0;
    for (int v : X) if (v > 0) activeFeatures++;
    std::cout << "  Active Markers       : " << activeFeatures << " / " << X.size() << " determinants active\n";
    std::cout << "+-------------------------------------------------------------------+\n\n";

    hasLastScan = true;
    lastScanTarget = "Direct_Terminal_Sequence (" + std::to_string(cleanSeq.length()) + " bp)";
    lastScanWasFile = false;
    lastScanBytes = cleanSeq.length();
    lastScanTimeMs = elapsedMs;
    lastExactHits = engine.getExactHits();
    lastMutatedHits = engine.getMutatedHits();
    lastResistanceScore = engine.getResistanceScore();
    lastFeatureVector = X;
    lastPredictions = predictions;
}

void REPLShell::executeTrieQuery(const std::string& kmer) {
    std::string querySeq = kmer;
    if (querySeq.empty()) {
        std::cout << "Enter nucleotide k-mer to query (e.g., 21 bp): " << std::flush;
        std::cin >> querySeq;
    }

    // Uppercase
    for (char& c : querySeq) c = std::toupper(static_cast<unsigned char>(c));

    std::cout << "\n========================================================================\n";
    std::cout << "                 4-ARY K-MER PREFIX TRIE DIAGNOSTICS                    \n";
    std::cout << "========================================================================\n";
    std::cout << "[*] Query Sequence   : " << querySeq << " (Length: " << querySeq.length() << " bp)\n";

    // 1. Bloom Filter Pre-screen
    bool bloomHit = trie.getBloomFilter().contains(querySeq);
    std::cout << "\n[1] 1 MB Fast-Rejection Bloom Filter (3 Hash Functions):\n";
    if (bloomHit) {
        std::cout << "    [+] Result: PASSED (k-mer bit-positions are set -> Proceed to Trie)\n";
    } else {
        std::cout << "    [-] Result: REJECTED in O(1) time (definitely NOT in AMR database)\n";
    }

    // 2. Exact Search (Hamming Distance = 0)
    std::string exactMeta;
    bool exactFound = trie.searchExact(querySeq, &exactMeta);
    std::cout << "\n[2] Exact 4-ary Prefix Tree Traversal (Hamming Distance = 0):\n";
    if (exactFound) {
        std::cout << "    [+] Status  : EXACT MATCH FOUND in O(K) time!\n";
        std::cout << "    [+] Metadata: " << exactMeta << "\n";
    } else {
        std::cout << "    [-] Status  : No exact match at tree depth " << querySeq.length() << "\n";
    }

    // 3. Mutation-Tolerant Search (Hamming Distance <= 1 SNP)
    std::string mutMeta;
    bool mutFound = trie.searchWithMismatch(querySeq, 1, &mutMeta);
    std::cout << "\n[3] Recursive Mutation Backtracking Search (Hamming Distance <= 1 SNP):\n";
    if (mutFound) {
        std::cout << "    [+] Status  : MUTATION-TOLERANT MATCH FOUND! (Within 1 SNP mismatch)\n";
        std::cout << "    [+] Metadata: " << mutMeta << "\n";
        std::cout << "    [+] Complexity: Poly O(3 * K^2) independent of database size N\n";
    } else {
        std::cout << "    [-] Status  : No match found within 1 SNP tolerance.\n";
    }

    std::cout << "========================================================================\n\n";
}

void REPLShell::executeMmapDiagnostics(const std::string& fastaFile) {
    std::string targetFile = fastaFile;
    if (targetFile.empty()) {
        std::string defFile = std::ifstream("data/sample_real.fasta").good() ? "data/sample_real.fasta" : "sample_real.fasta";
        std::cout << "Enter FASTA file path to benchmark memory mapping [default: " << defFile << "]: " << std::flush;
        std::getline(std::cin, targetFile);
        if (targetFile.empty()) targetFile = defFile;
    }

    std::ifstream checkFile(targetFile);
    if (!checkFile.good()) {
        std::cerr << "[!] Error: File '" << targetFile << "' not found.\n";
        return;
    }
    checkFile.close();

    std::cout << "\n========================================================================\n";
    std::cout << "            MEMORY-MAPPED ZERO-COPY I/O ARCHITECTURE AUDIT              \n";
    std::cout << "========================================================================\n";
    try {
        auto t0 = std::chrono::high_resolution_clock::now();
        FastaMmapParser parser(targetFile);
        auto t1 = std::chrono::high_resolution_clock::now();
        double initTimeUs = std::chrono::duration<double, std::micro>(t1 - t0).count();

        size_t fileSize = parser.getFileSize();
        bool isMmap = parser.isMemoryMapped();

        std::cout << "  Target File Path     : " << targetFile << "\n";
        std::cout << "  File Size On Disk    : " << fileSize << " bytes (" 
                  << std::fixed << std::setprecision(2) << (fileSize / 1024.0) << " KB)\n";
        std::cout << "  Virtual Memory Mapping: " 
                  << (isMmap ? "Win32 CreateFileMapping / MapViewOfFile (Active)" : "POSIX mmap / Standard Stream") << "\n";
        std::cout << "  Mapping Initialization: " << std::fixed << std::setprecision(1) << initTimeUs << " microseconds\n";
        std::cout << "  Chunk Paging Size    : 4,096 bytes (Hardware 4 KB memory page aligned)\n";
        std::cout << "  Overlap Buffer Size  : 20 bytes (K - 1 for 21-mers, 0% boundary miss rate)\n";
        std::cout << "  Auxiliary RAM Footprint: O(1) constant streaming memory\n";

        // Benchmark streaming read
        std::string chunk;
        size_t chunksRead = 0;
        size_t bytesRead = 0;
        auto readStart = std::chrono::high_resolution_clock::now();
        while (parser.getNextChunk(chunk, 4096, 20)) {
            chunksRead++;
            bytesRead += chunk.size();
        }
        auto readEnd = std::chrono::high_resolution_clock::now();
        double streamMs = std::chrono::duration<double, std::milli>(readEnd - readStart).count();
        double mbps = (bytesRead / 1024.0 / 1024.0) / (streamMs / 1000.0);

        std::cout << "\n  --- Streaming Throughput Telemetry ---\n";
        std::cout << "  Total Chunks Streamed : " << chunksRead << "\n";
        std::cout << "  Bytes Ingested        : " << bytesRead << " bytes\n";
        std::cout << "  Stream Wall-Clock Time: " << std::fixed << std::setprecision(3) << streamMs << " ms\n";
        if (streamMs > 0.0) {
            std::cout << "  I/O Memory Throughput : " << std::fixed << std::setprecision(2) << mbps << " MB/s\n";
        }
        std::cout << "========================================================================\n\n";

    } catch (const std::exception& e) {
        std::cerr << "[!] Memory mapping audit error: " << e.what() << "\n";
    }
}

void REPLShell::printScoresAndFeatures() {
    if (!hasLastScan) {
        std::cout << "\n[!] No scan has been run in this session yet.\n";
        std::cout << "    Run 'scan sample_real.fasta' or option [1] to generate test scores and feature vectors.\n\n";
        return;
    }

    std::cout << "\n========================================================================\n";
    std::cout << "            TEST SCORES & BINARY FEATURE VECTOR X AUDIT                 \n";
    std::cout << "========================================================================\n";
    std::cout << "  Scanned Target       : " << lastScanTarget << "\n";
    std::cout << "  Bytes Processed      : " << lastScanBytes << " bytes\n";
    std::cout << "  Inference Latency    : " << std::fixed << std::setprecision(2) << lastScanTimeMs << " ms\n\n";

    std::cout << "  --- Summary Scores ---\n";
    std::cout << "  [+] Exact Hits (Hamming = 0)   : " << lastExactHits << "\n";
    std::cout << "  [+] Mutated Hits (Hamming <= 1): " << lastMutatedHits << "\n";
    std::cout << "  [+] Total Resistance Score     : " << lastResistanceScore << " points\n\n";

    const auto& records = db.getRecords();
    std::cout << "  --- Target-Gated Binary Feature Vector X (Dimension: " << lastFeatureVector.size() << ") ---\n";
    std::cout << "  Index | Value | Gene Marker ID | Target Antibiotic | Record Type\n";
    std::cout << "  ------+-------+----------------+-------------------+-------------\n";
    int activeCount = 0;
    for (size_t i = 0; i < lastFeatureVector.size(); ++i) {
        int val = lastFeatureVector[i];
        if (val > 0) {
            activeCount++;
            std::string markerId = (i < records.size()) ? records[i].markerId : "unknown";
            std::string drug = (i < records.size()) ? records[i].targetDrug : "unknown";
            std::string type = (i < records.size()) ? records[i].recordType : "gene";
            std::cout << "   " << std::setw(4) << i << " |   " << val << "   | "
                      << std::setw(14) << std::left << markerId << " | "
                      << std::setw(17) << std::left << drug << " | "
                      << type << "\n";
        }
    }
    std::cout << "\n  [i] Total Active Determinants in X: " << activeCount << " of " << lastFeatureVector.size() << "\n";
    std::cout << "========================================================================\n\n";
}

void REPLShell::configureFeatures() {
    std::cout << "\n+----------------------------------------------------------------------+\n";
    std::cout << "|                 CONFIGURE SYSTEM FEATURES WIZARD                     |\n";
    std::cout << "+----------------------------------------------------------------------+\n";
    std::cout << "  [1] Toggle Mutation Tolerance (Fuzzy Hamming <= 1) : Currently " 
              << (fuzzyEnabled ? "ENABLED" : "DISABLED") << "\n";
    std::cout << "  [2] Toggle Multithreading Concurrency              : Currently " 
              << (asyncEnabled ? "ENABLED" : "DISABLED") << "\n";
    std::cout << "  [3] Set Decision Confidence Threshold (%)          : Currently " 
              << confidenceThreshold << "%\n";
    std::cout << "  [4] Change Reference AMR Database Catalog          : Currently " 
              << currentDbFile << "\n";
    std::cout << "  [5] Return to Main Terminal Menu\n";
    std::cout << "+----------------------------------------------------------------------+\n";
    std::cout << "Select option [1-5]: " << std::flush;

    std::string choice;
    if (std::getline(std::cin, choice)) {
        if (choice == "1") {
            fuzzyEnabled = !fuzzyEnabled;
            std::cout << "[+] Mutation-Tolerant matching is now: " 
                      << (fuzzyEnabled ? "ENABLED (Hamming <= 1 SNP)" : "DISABLED (Exact Only)") << "\n";
        } else if (choice == "2") {
            asyncEnabled = !asyncEnabled;
            std::cout << "[+] Multithreaded chunk concurrency is now: " 
                      << (asyncEnabled ? "ENABLED (Native Threads)" : "DISABLED (Sequential)") << "\n";
        } else if (choice == "3") {
            std::cout << "Enter new confidence percentage (e.g., 80.0): " << std::flush;
            double val;
            if (std::cin >> val) {
                confidenceThreshold = val;
                std::cout << "[+] Confidence threshold set to: " << confidenceThreshold << "%\n";
            }
            std::string dummy;
            std::getline(std::cin, dummy);
        } else if (choice == "4") {
            std::cout << "Enter new database file path (e.g., markers_large.csv, markers.csv): " << std::flush;
            std::string path;
            if (std::getline(std::cin, path) && !path.empty()) {
                std::ifstream test(path);
                if (test.good()) {
                    currentDbFile = path;
                    trie.clear();
                    db.loadFromFile(path, trie);
                    std::cout << "[+] Successfully loaded database: " << path << "\n";
                } else {
                    std::cerr << "[!] Error: File '" << path << "' not found.\n";
                }
            }
        }
    }
}

void REPLShell::run() {
    printWelcomeBanner();
    std::string line;

    while (true) {
        std::cout << "genome_firewall> " << std::flush;
        if (!std::getline(std::cin, line)) break;

        // Trim leading and trailing whitespace
        size_t first = line.find_first_not_of(" \t\r\n");
        if (first == std::string::npos) continue;
        size_t last = line.find_last_not_of(" \t\r\n");
        line = line.substr(first, (last - first + 1));

        std::stringstream ss(line);
        std::string cmd;
        ss >> cmd;

        // Numbered Menu Choices
        if (cmd == "1") {
            executeScan("");
        } else if (cmd == "2") {
            executeSequenceScan("");
        } else if (cmd == "3") {
            executeTrieQuery("");
        } else if (cmd == "4") {
            executeMmapDiagnostics("");
        } else if (cmd == "5") {
            printScoresAndFeatures();
        } else if (cmd == "6") {
            configureFeatures();
        } else if (cmd == "7" || cmd == "demo") {
            std::string defDemo = std::ifstream("data/sample_real.fasta").good() ? "data/sample_real.fasta" : "sample_real.fasta";
            executeScan(defDemo);
        } else if (cmd == "8" || cmd == "status") {
            printStatus();
        } else if (cmd == "9" || cmd == "exit" || cmd == "quit") {
            std::cout << "Exiting Genome Firewall terminal session. Goodbye!\n";
            break;
        } else if (cmd == "menu") {
            printMenu();
        } else if (cmd == "help") {
            printHelp();
        } else if (cmd == "scan") {
            std::string path;
            if (ss >> path) {
                executeScan(path);
            } else {
                executeScan("");
            }
        } else if (cmd == "scan_seq" || cmd == "seq") {
            std::string seq;
            if (ss >> seq) {
                executeSequenceScan(seq);
            } else {
                executeSequenceScan("");
            }
        } else if (cmd == "query" || cmd == "trie" || cmd == "query_trie") {
            std::string kmer;
            if (ss >> kmer) {
                executeTrieQuery(kmer);
            } else {
                executeTrieQuery("");
            }
        } else if (cmd == "mmap") {
            std::string path;
            if (ss >> path) {
                executeMmapDiagnostics(path);
            } else {
                executeMmapDiagnostics("");
            }
        } else if (cmd == "scores" || cmd == "features") {
            printScoresAndFeatures();
        } else if (cmd == "config") {
            configureFeatures();
        } else if (cmd == "load_db") {
            std::string path;
            if (ss >> path) {
                currentDbFile = path;
                trie.clear();
                db.loadFromFile(path, trie);
            } else {
                std::cout << "Usage: load_db <csv_file>\n";
            }
        } else if (cmd == "set_fuzzy") {
            std::string opt;
            if (ss >> opt) {
                fuzzyEnabled = (opt == "on" || opt == "1" || opt == "true");
                std::cout << "Fuzzy matching set to: " << (fuzzyEnabled ? "ON" : "OFF") << "\n";
            } else {
                fuzzyEnabled = !fuzzyEnabled;
                std::cout << "Fuzzy matching toggled to: " << (fuzzyEnabled ? "ON" : "OFF") << "\n";
            }
        } else if (cmd == "set_async") {
            std::string opt;
            if (ss >> opt) {
                asyncEnabled = (opt == "on" || opt == "1" || opt == "true");
                std::cout << "Multithreading set to: " << (asyncEnabled ? "ON" : "OFF") << "\n";
            } else {
                asyncEnabled = !asyncEnabled;
                std::cout << "Multithreading toggled to: " << (asyncEnabled ? "ON" : "OFF") << "\n";
            }
        } else if (cmd == "set_conf") {
            double val;
            if (ss >> val) {
                confidenceThreshold = val;
                std::cout << "Confidence threshold set to: " << confidenceThreshold << "%\n";
            } else {
                std::cout << "Usage: set_conf <percentage>\n";
            }
        } else {
            std::cout << "Unknown command: '" << cmd << "'. Type 'menu' or 'help' for options.\n";
        }
    }
}
