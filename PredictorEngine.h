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
    NO_CALL         // 0.15 <= P <= 0.85 (Uncertain evidence / statistical association)
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
    int directHits;
    int mutatedHits;
    bool targetPresent;
    std::string clinicalRecommendation;
};

class PredictorEngine {
public:
    explicit PredictorEngine(const AMRTrie& trie, double confidenceThreshold = 75.0);

    // Module 01: Feature Extraction
    void analyzeSequenceFeatures(const std::string& sequence, size_t kmerLength, bool enableFuzzy);

    // Module 02 & 03: Evaluate target-gated predictions using Logistic Regression
    std::vector<DrugPrediction> evaluatePredictionsWithModel(const AMRDatabase& db, const std::vector<int>& featureVector) const;

    std::string generateASCIIReport(const std::vector<DrugPrediction>& predictions, double elapsedMs, size_t totalBytesScanned) const;

    const LogisticRegressor& getModel() const { return regressor; }

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
