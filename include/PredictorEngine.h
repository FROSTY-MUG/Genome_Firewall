#ifndef PREDICTOR_ENGINE_H
#define PREDICTOR_ENGINE_H

#include <string>
#include <vector>
#include <unordered_map>
#include "AMRTrie.h"
#include "AMRDatabase.h"
#include "LogisticRegressor.h"

enum class PredictionStatus {
    LIKELY_TO_FAIL, // P > 0.85 (High confidence of resistance)
    LIKELY_TO_WORK, // P < 0.15 AND Target Intact (High confidence of susceptibility)
    NO_CALL         // Uncertain evidence, or a call below the confidence threshold
};

enum class EvidenceTier {
    TIER_I,   // Direct match to validated AMR gene or point mutation
    TIER_II,  // Statistical association score based on multi-marker profile
    TIER_III  // Absence of resistance signals coupled with confirmed molecular target integrity
};

struct DrugPrediction {
    std::string drugName;
    PredictionStatus status;
    double probability;     // Logistic regression probability P (0.0 to 1.0)
    double confidenceScore; // Scaled confidence percentage (0.0 to 100.0%)
    EvidenceTier tier;
    int directHits{0};
    int mutatedHits{0};
    bool targetPresent{false};
    std::string clinicalRecommendation;
};

// Everything the machine-readable report needs, beyond the predictions themselves.
struct ScanContext {
    std::string inputFile;
    std::string databaseFile;
    std::string systemMode;   // "fuzzy" | "exact"
    std::string threading;    // "async" | "serial"
    double confidenceThreshold{75.0};

    double dbTimeMs{0.0};
    double scanTimeMs{0.0};
    double totalTimeMs{0.0};

    size_t totalBytesScanned{0};
    size_t chunkCount{0};
    size_t kmerLength{21};
    size_t chunkSize{4096};

    size_t markerCount{0};
    size_t trieNodes{0};
    size_t trieMemoryKB{0};
    size_t bloomBits{0};

    size_t exactHits{0};
    size_t mutatedHits{0};
};

class PredictorEngine {
public:
    explicit PredictorEngine(const AMRTrie& trie, double confidenceThreshold = 75.0);

    // Module 01: Sequence Feature Extraction (legacy/fallback path)
    void analyzeSequenceFeatures(const std::string& sequence, size_t kmerLength, bool enableFuzzy);

    // Direct thread-safe hit accumulator (avoids redundant second sequence pass)
    void mergeHits(const std::unordered_map<std::string, int>& direct,
                   const std::unordered_map<std::string, int>& mutated,
                   const std::unordered_map<std::string, bool>& targetIntegrity,
                   int directCount, int mutatedCount);

    // Module 02 & 03: Evaluate target-gated predictions using Logistic Regression
    std::vector<DrugPrediction> evaluatePredictionsWithModel(const AMRDatabase& db, const std::vector<int>& featureVector) const;

    std::string generateASCIIReport(const std::vector<DrugPrediction>& predictions, double elapsedMs, size_t totalBytesScanned) const;

    // Machine-readable report consumed by the web frontend / API server.
    std::string generateJSONReport(const std::vector<DrugPrediction>& predictions, const ScanContext& ctx) const;

    const LogisticRegressor& getModel() const { return regressor; }

    int getDirectHitCount() const { return directHitCount; }
    int getMutatedHitCount() const { return mutatedHitCount; }

private:
    const AMRTrie& trie;
    double confidenceThreshold;
    mutable LogisticRegressor regressor;

    int directHitCount{0};
    int mutatedHitCount{0};
    std::unordered_map<std::string, int> drugDirectMatches;
    std::unordered_map<std::string, int> drugMutatedMatches;
    std::unordered_map<std::string, bool> drugTargetIntegrity;

    std::string statusToString(PredictionStatus status) const;
    std::string tierToString(EvidenceTier tier) const;
};

#endif // PREDICTOR_ENGINE_H
