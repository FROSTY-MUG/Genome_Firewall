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
        std::getline(file, line);
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
        std::getline(file, line);
        while (std::getline(file, line)) {
            if (line.empty()) continue;
            std::stringstream ss(line);
            std::string id, seq, drug;
            std::getline(ss, id, ',');
            std::getline(ss, seq, ',');
            std::getline(ss, drug, ',');
            records.push_back({id, seq, drug, "Gene", "CSV_DB"});
        }
        return records;
    }
};

std::unique_ptr<IDatabaseParser> AMRDatabase::getParser(DBFormat format) {
    (void)format;
    return nullptr; // Controlled dynamically in loadFromFile
}

void AMRDatabase::loadFromFile(const std::string& filepath, AMRTrie& trie) {
    std::unique_ptr<IDatabaseParser> parser;
    
    // Dynamically Route by File Extension
    if (filepath.find(".fasta") != std::string::npos || filepath.find(".fsa") != std::string::npos) {
        parser = std::make_unique<FastaDBParser>();
        std::cout << "[*] Routing to Native FASTA Parser for: " << filepath << "\n";
    } else if (filepath.find(".tsv") != std::string::npos) {
        parser = std::make_unique<TSVParser>();
        std::cout << "[*] Routing to Native TSV Parser for: " << filepath << "\n";
    } else {
        parser = std::make_unique<CSVParser>();
        std::cout << "[*] Routing to Native CSV Parser for: " << filepath << "\n";
    }

    auto newRecords = parser->parse(filepath);
    int added = 0;
    for (const auto& rec : newRecords) {
        if (rec.sequence.length() >= 3) { // Ensure sequence integrity
            databaseRecords.push_back(rec);
            trie.insertMarker(rec.sequence, rec.markerId, rec.targetDrug);
            added++;
        }
    }
    std::cout << "[+] Successfully ingested " << added << " markers into the Trie Arena.\n";
}

const std::vector<AMRRecord>& AMRDatabase::getRecords() const { return databaseRecords; }
size_t AMRDatabase::getMarkerCount() const { return databaseRecords.size(); }
