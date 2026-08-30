#include "ScoringEngine.h"
#include <iostream>

ScoringEngine::ScoringEngine(const AMRTrie& trie, const AMRDatabase& db, bool enableFuzzy, double confidenceThreshold)
    : amrTrie(trie),
      amrDb(db),
      fuzzyMatchEnabled(enableFuzzy),
      predictor(trie, confidenceThreshold) {
    size_t numRecords = amrDb.getRecords().size();
    if (numRecords == 0) numRecords = 10;
    featureVectorAtomics = std::vector<std::atomic<int>>(numRecords);
    for (auto& atom : featureVectorAtomics) {
        atom.store(0, std::memory_order_relaxed);
    }
}

void ScoringEngine::processSequenceChunk(const std::string& sequence, size_t kmerLength) {
    if (sequence.length() < kmerLength) return;

    int localExact = 0;
    int localMutated = 0;

    const auto& records = amrDb.getRecords();

    for (size_t i = 0; i <= sequence.length() - kmerLength; ++i) {
        std::string kmer = sequence.substr(i, kmerLength);

        std::string meta;
        if (amrTrie.searchExact(kmer, &meta)) {
            localExact++;
            
            // Map detected k-mer to index in Binary Feature Vector X
            for (size_t idx = 0; idx < records.size(); ++idx) {
                if (records[idx].sequence.find(kmer) != std::string::npos || 
                    (!meta.empty() && meta.find(records[idx].markerId) != std::string::npos)) {
                    featureVectorAtomics[idx].store(1, std::memory_order_relaxed);
                }
            }
        } else if (fuzzyMatchEnabled && amrTrie.searchWithMismatch(kmer, 1, &meta)) {
            localMutated++;

            for (size_t idx = 0; idx < records.size(); ++idx) {
                if (!meta.empty() && meta.find(records[idx].markerId) != std::string::npos) {
                    featureVectorAtomics[idx].store(1, std::memory_order_relaxed);
                }
            }
        }
    }

    predictor.analyzeSequenceFeatures(sequence, kmerLength, fuzzyMatchEnabled);

    if (localExact > 0) exactMatches.fetch_add(localExact, std::memory_order_relaxed);
    if (localMutated > 0) mutatedMatches.fetch_add(localMutated, std::memory_order_relaxed);

    int localScore = localExact + localMutated;
    if (localScore > 0) {
        resistanceScore.fetch_add(localScore, std::memory_order_relaxed);
    }
}

void ScoringEngine::processSequenceChunkAsync(const std::string& sequence, size_t kmerLength) {
    std::lock_guard<std::mutex> lock(futuresMutex);
    asyncFutures.push_back(std::async(std::launch::async, [this, sequence, kmerLength]() {
        this->processSequenceChunk(sequence, kmerLength);
    }));
}

void ScoringEngine::waitForAsyncTasks() {
    std::lock_guard<std::mutex> lock(futuresMutex);
    for (auto& fut : asyncFutures) {
        if (fut.valid()) {
            fut.get();
        }
    }
    asyncFutures.clear();
}

std::vector<int> ScoringEngine::getBinaryFeatureVector() const {
    std::vector<int> result(featureVectorAtomics.size());
    for (size_t i = 0; i < featureVectorAtomics.size(); ++i) {
        result[i] = featureVectorAtomics[i].load(std::memory_order_relaxed);
    }
    return result;
}
