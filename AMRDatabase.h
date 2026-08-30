#pragma once
#include <string>
#include <vector>
#include <memory>
#include "AMRTrie.h"

struct AMRRecord {
    std::string markerId;   // Reverted from 'id' to fix ScoringEngine error
    std::string sequence;
    std::string targetDrug; // Reverted from 'antibioticTarget' to fix PredictorEngine error
    std::string recordType;
    std::string sourceDB;
};

enum class DBFormat { AMRFINDER_TSV, RESFINDER_FSA, BVBRC_CSV, LEGACY_MARKERS };

class IDatabaseParser {
public:
    virtual ~IDatabaseParser() = default;
    virtual std::vector<AMRRecord> parse(const std::string& filepath) = 0;
};

class AMRDatabase {
private:
    std::vector<AMRRecord> databaseRecords;
    std::unique_ptr<IDatabaseParser> getParser(DBFormat format);

public:
    AMRDatabase() = default;
    ~AMRDatabase() = default;

    // Bridged API to match your REPLShell and main.cpp exactly
    void loadFromFile(const std::string& filepath, AMRTrie& trie);
    size_t getMarkerCount() const;
    const std::vector<AMRRecord>& getRecords() const;
};
