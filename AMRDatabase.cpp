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

    // ---------------------------------------------------------------------------
    // K-MER INDEXING STRATEGY
    //
    // The scoring engine slides a 21-mer window over the genome and calls
    // trie.searchExact(kmer). searchExact only returns true when isEndOfMarker
    // is set — which the trie sets at the END of each inserted string.
    //
    // If we insert the full 80-90bp marker sequences, the trie would only match
    // a 21-mer at depth 21 ONLY IF isEndOfMarker is set there — which it never
    // is for full-length markers.
    //
    // FIX: Extract all overlapping 21-mers from each marker and insert those
    // as individual entries. This is the correct k-mer indexing approach used
    // in bioinformatics tools (Kallisto, HISAT2, etc.).
    // ---------------------------------------------------------------------------
    static const size_t KMER_LEN = 21;

    int added     = 0;
    int kmerTotal = 0;

    for (const auto& rec : newRecords) {
        if (rec.sequence.length() < KMER_LEN) continue;

        databaseRecords.push_back(rec);

        // Build pipe-delimited metadata: "id|type|drug|recType"
        // PredictorEngine::analyzeSequenceFeatures parses exactly this format
        std::string meta = rec.markerId + "|" + rec.recordType + "|"
                         + rec.targetDrug + "|ResistanceGene";

        // Index every overlapping 21-mer from the marker sequence
        for (size_t i = 0; i <= rec.sequence.length() - KMER_LEN; ++i) {
            std::string kmer = rec.sequence.substr(i, KMER_LEN);
            trie.insertMarker(kmer, meta, rec.targetDrug, rec.recordType);
            ++kmerTotal;
        }
        ++added;
    }
    std::cout << "[+] Successfully ingested " << added << " markers ("
              << kmerTotal << " unique k-mers) into the Trie Arena.\n";
}

const std::vector<AMRRecord>& AMRDatabase::getRecords() const { return databaseRecords; }
size_t AMRDatabase::getMarkerCount() const { return databaseRecords.size(); }
