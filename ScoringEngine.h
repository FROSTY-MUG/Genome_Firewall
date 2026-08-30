#ifndef SCORING_ENGINE_H
#define SCORING_ENGINE_H

#include <string>
#include <atomic>
#include <vector>
#include <future>
#include <mutex>
#include "AMRTrie.h"
#include "PredictorEngine.h"
#include "AMRDatabase.h"

class ScoringEngine {
public:
    explicit ScoringEngine(const AMRTrie& trie, const AMRDatabase& db, bool enableFuzzy = true, double confidenceThreshold = 75.0);
    ~ScoringEngine() = default;

    ScoringEngine(const ScoringEngine&) = delete;
    ScoringEngine& operator=(const ScoringEngine&) = delete;

    void processSequenceChunk(const std::string& sequence, size_t kmerLength);
    void processSequenceChunkAsync(const std::string& sequence, size_t kmerLength);
    void waitForAsyncTasks();

    // Get the generated Binary Feature Vector X (1 = present, 0 = absent)
    std::vector<int> getBinaryFeatureVector() const;

    int getResistanceScore() const { return resistanceScore.load(std::memory_order_relaxed); }
    int getExactHits() const { return exactMatches.load(std::memory_order_relaxed); }
    int getMutatedHits() const { return mutatedMatches.load(std::memory_order_relaxed); }

    PredictorEngine& getPredictor() { return predictor; }
    const PredictorEngine& getPredictor() const { return predictor; }

private:
    const AMRTrie& amrTrie;
    const AMRDatabase& amrDb;
    bool fuzzyMatchEnabled;

    std::atomic<int> resistanceScore{0};
    std::atomic<int> exactMatches{0};
    std::atomic<int> mutatedMatches{0};

    // Thread-safe Binary Feature Vector X
    std::vector<std::atomic<int>> featureVectorAtomics;

    std::vector<std::future<void>> asyncFutures;
    mutable std::mutex futuresMutex;

    PredictorEngine predictor;
};

#endif // SCORING_ENGINE_H
