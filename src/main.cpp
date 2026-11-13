#include <iostream>
#include <string>
#include <vector>
#include <chrono>
#include <iomanip>
#include <stdexcept>
#include "AMRTrie.h"
#include "AMRDatabase.h"
#include "FastaMmapParser.h"
#include "ScoringEngine.h"
#include "PredictorEngine.h"
#include "REPLShell.h"

static std::string escapeJson(const std::string& value) {
    std::string out;
    out.reserve(value.size() + 8);
    for (char c : value) {
        switch (c) {
            case '"':  out += "\\\""; break;
            case '\\': out += "\\\\"; break;
            case '\n': out += "\\n";  break;
            case '\r': out += "\\r";  break;
            case '\t': out += "\\t";  break;
            default:   out += c;
        }
    }
    return out;
}

static void printBanner() {
    std::cerr << "========================================================================\n";
    std::cerr << "               GENOME FIREWALL v2.0 - BIOSECURITY CLI                   \n";
    std::cerr << "       Predictive Antimicrobial Resistance Decision Engine              \n";
    std::cerr << "========================================================================\n";
}

static void printUsage(const char* progName) {
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
    std::cerr << "  --json                Emit a machine-readable JSON report on stdout\n";
    std::cerr << "  --interactive         Launch interactive REPL terminal session\n\n";
}

int main(int argc, char* argv[]) {
    std::string fastaFile = "sample.fasta";
    std::string amrDbFile = "markers.csv";
    bool enableFuzzy = true;
    bool enableAsync = true;
    bool interactiveMode = false;
    bool enableJson = false;
    double confidenceThreshold = 75.0;

    for (int i = 1; i < argc; ++i) {
        std::string arg = argv[i];
        if (arg == "--json") {
            enableJson = true;
        } else if (arg == "--interactive" || arg == "-i") {
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
            try {
                confidenceThreshold = std::stod(argv[++i]);
            } catch (...) {
                std::cerr << "[!] Invalid value for --confidence, using default 75.0\n";
                confidenceThreshold = 75.0;
            }
            if (confidenceThreshold < 0.0) confidenceThreshold = 0.0;
            if (confidenceThreshold > 100.0) confidenceThreshold = 100.0;
        } else if (arg == "--help" || arg == "-h") {
            printUsage(argv[0]);
            return 0;
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

    // Diagnostics always go to stderr so stdout stays clean for --json piping.
    printBanner();
    std::cerr << "[*] System Mode: " << (enableFuzzy ? "Mutation-Tolerant (Hamming <= 1)" : "Exact Match Only (Hamming = 0)")
              << " | " << (enableAsync ? "Multithreaded (Native OS Threads)" : "Sequential (Single-threaded)") << "\n";
    std::cerr << "[*] Confidence Threshold: " << confidenceThreshold << "%\n\n";

    auto totalStart = std::chrono::high_resolution_clock::now();

    try {
        // 1. Initialize Trie & Database
        AMRTrie trie;
        AMRDatabase db;

        std::cerr << "[*] Loading AMR Reference Catalog from: " << amrDbFile << "...\n";
        auto dbStart = std::chrono::high_resolution_clock::now();
        db.loadFromFile(amrDbFile, trie);
        auto dbEnd = std::chrono::high_resolution_clock::now();
        double dbTimeMs = std::chrono::duration<double, std::milli>(dbEnd - dbStart).count();

        if (db.getMarkerCount() == 0) {
            throw std::runtime_error("No usable markers found in database: " + amrDbFile);
        }

        std::cerr << "    -> Indexed " << db.getMarkerCount() << " AMR signatures in 4-ary Prefix Trie ("
                  << trie.getNodeCount() << " nodes, Arena: " << (trie.getMemoryUsage() / 1024) << " KB)\n";
        std::cerr << "    -> Fast-Rejection Bloom Filter initialized (" << trie.getBloomFilter().getBitSize() << " bits)\n";
        std::cerr << "    -> Load & Index Time: " << std::fixed << std::setprecision(3) << dbTimeMs << " ms\n\n";

        // 2. Initialize Zero-Copy FASTA Parser & Scoring Engine
        std::cerr << "[*] Initializing Zero-Copy Sequence Ingestion for: " << fastaFile << "...\n";
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

        std::cerr << "[+] Scan completed across " << chunkCount << " chunks ("
                  << (totalBytesParsed / 1024) << " KB scanned).\n";

        // 3. Extract Binary Feature Vector X & Evaluate Logistic Regression Predictions
        std::vector<int> X = engine.getBinaryFeatureVector();
        auto predictions = engine.getPredictor().evaluatePredictionsWithModel(db, X);

        auto totalEnd = std::chrono::high_resolution_clock::now();
        double totalTimeMs = std::chrono::duration<double, std::milli>(totalEnd - totalStart).count();

        if (enableJson) {
            ScanContext ctx;
            ctx.inputFile = fastaFile;
            ctx.databaseFile = amrDbFile;
            ctx.systemMode = enableFuzzy ? "fuzzy" : "exact";
            ctx.threading = enableAsync ? "async" : "serial";
            ctx.confidenceThreshold = confidenceThreshold;

            ctx.dbTimeMs = dbTimeMs;
            ctx.scanTimeMs = scanTimeMs;
            ctx.totalTimeMs = totalTimeMs;

            ctx.totalBytesScanned = totalBytesParsed;
            ctx.chunkCount = chunkCount;
            ctx.kmerLength = KMER_LENGTH;
            ctx.chunkSize = CHUNK_SIZE;

            ctx.markerCount = db.getMarkerCount();
            ctx.trieNodes = trie.getNodeCount();
            ctx.trieMemoryKB = trie.getMemoryUsage() / 1024;
            ctx.bloomBits = trie.getBloomFilter().getBitSize();

            ctx.exactHits = static_cast<size_t>(engine.getExactHits());
            ctx.mutatedHits = static_cast<size_t>(engine.getMutatedHits());

            std::cout << engine.getPredictor().generateJSONReport(predictions, ctx) << "\n";
        } else {
            std::string report = engine.getPredictor().generateASCIIReport(predictions, scanTimeMs, totalBytesParsed);
            std::cout << report;
            std::cerr << "[*] Total Execution Wall-Clock Time: " << std::fixed << std::setprecision(2) << totalTimeMs << " ms\n\n";
        }

    } catch (const std::exception& e) {
        if (enableJson) {
            std::cout << "{\"error\":\"scan_failed\",\"message\":\"" << escapeJson(e.what()) << "\"}\n";
        } else {
            std::cerr << "\n[!] FATAL ERROR: " << e.what() << "\n";
        }
        return 1;
    }

    return 0;
}
