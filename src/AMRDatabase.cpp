#include "AMRDatabase.h"
#include <iostream>
#include <fstream>
#include <sstream>
#include <algorithm>

// --- UNIVERSAL PARSERS ---

class FastaDBParser : public IDatabaseParser {
public:
    std::vector<AMRRecord> parse(const std::string& filepath) override {
        std::vector<AMRRecord> records;
        std::ifstream file(filepath);
        if (!file.is_open()) return records;
        std::string line, id, seq;
        while (std::getline(file, line)) {
            if (line.empty()) continue;
            if (line[0] == '>') {
                if (!seq.empty()) {
                    records.push_back({id, seq, "Broad-Spectrum", "Gene", "FASTA_DB"});
                    seq.clear();
                }
                id = line.substr(1, line.find_first_of(" \t") - 1);
            } else {
                seq += line;
            }
        }
        if (!seq.empty()) records.push_back({id, seq, "Broad-Spectrum", "Gene", "FASTA_DB"});
        return records;
    }
};

class TSVParser : public IDatabaseParser {
public:
    std::vector<AMRRecord> parse(const std::string& filepath) override {
        std::vector<AMRRecord> records;
        std::ifstream file(filepath);
        if (!file.is_open()) return records;
        std::string line;
        std::getline(file, line); // Header
        while (std::getline(file, line)) {
            if (line.empty()) continue;
            std::stringstream ss(line);
            std::string token;
            std::vector<std::string> cols;
            while (std::getline(ss, token, '\t')) cols.push_back(token);
            if (cols.size() >= 3) records.push_back({cols[0], cols[1], cols[2], "TSV_Marker", "AMRFinder"});
        }
        return records;
    }
};

class CSVParser : public IDatabaseParser {
public:
    std::vector<AMRRecord> parse(const std::string& filepath) override {
        std::vector<AMRRecord> records;
        std::ifstream file(filepath);
        if (!file.is_open()) return records;
        std::string line;
        std::getline(file, line); // Header
        while (std::getline(file, line)) {
            if (line.empty()) continue;
            std::stringstream ss(line);
            std::string id, seq, drug;
            std::getline(ss, id, ',');
            std::getline(ss, seq, ',');
            std::getline(ss, drug, ',');
            if (!id.empty() && !seq.empty()) {
                records.push_back({id, seq, drug.empty() ? "Broad-Spectrum" : drug, "Gene", "CSV_DB"});
            }
        }
        return records;
    }
};

std::unique_ptr<IDatabaseParser> AMRDatabase::getParser(DBFormat format) {
    (void)format;
    return nullptr;
}

static std::string resolveDbPath(const std::string& filepath) {
    std::ifstream test(filepath);
    if (test.good()) return filepath;
    std::string fallback = "data/" + filepath;
    std::ifstream test2(fallback);
    if (test2.good()) return fallback;
    return filepath;
}

void AMRDatabase::loadFromFile(const std::string& filepath, AMRTrie& trie) {
    databaseRecords.clear();
    std::string actualPath = resolveDbPath(filepath);
    std::unique_ptr<IDatabaseParser> parser;

    if (actualPath.find(".fasta") != std::string::npos || actualPath.find(".fsa") != std::string::npos) {
        parser = std::make_unique<FastaDBParser>();
        std::cout << "[*] Routing to Native FASTA Parser for: " << actualPath << "\n";
    } else if (actualPath.find(".tsv") != std::string::npos) {
        parser = std::make_unique<TSVParser>();
        std::cout << "[*] Routing to Native TSV Parser for: " << actualPath << "\n";
    } else {
        parser = std::make_unique<CSVParser>();
        std::cout << "[*] Routing to Native CSV Parser for: " << actualPath << "\n";
    }

    auto newRecords = parser->parse(actualPath);
    if (newRecords.empty()) {
        std::cerr << "[!] Warning: No records parsed from database file: " << actualPath << "\n";
        return;
    }

    static const size_t KMER_LEN = 21;
    int added = 0;
    int kmerTotal = 0;

    databaseRecords.reserve(newRecords.size());

    for (const auto& rec : newRecords) {
        if (rec.sequence.length() < KMER_LEN) continue;

        int recordIdx = static_cast<int>(databaseRecords.size());
        databaseRecords.push_back(rec);

        // Index all overlapping 21-mers into Trie with zero-allocation pointer slices
        const char* seqData = rec.sequence.data();
        const size_t seqLen = rec.sequence.length();
        for (size_t i = 0; i <= seqLen - KMER_LEN; ++i) {
            trie.insertMarker(seqData + i, KMER_LEN, recordIdx);
            ++kmerTotal;
        }
        ++added;
    }
    std::cout << "[+] Successfully ingested " << added << " markers ("
              << kmerTotal << " unique k-mers) into the Trie Arena.\n";
}

const std::vector<AMRRecord>& AMRDatabase::getRecords() const { return databaseRecords; }
size_t AMRDatabase::getMarkerCount() const { return databaseRecords.size(); }
