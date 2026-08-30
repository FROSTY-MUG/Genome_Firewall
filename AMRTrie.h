#ifndef AMR_TRIE_H
#define AMR_TRIE_H

#include <string>
#include <vector>
#include <memory>
#include "TrieArena.h"
#include "AMRBloomFilter.h"

struct TrieNode {
    TrieNode* children[4]{nullptr, nullptr, nullptr, nullptr};
    bool isEndOfMarker{false};
    std::string markerMetadata{};
    std::string targetDrug{};
    std::string mechanism{};
};

class AMRTrie {
public:
    AMRTrie();
    ~AMRTrie() = default;

    AMRTrie(const AMRTrie&) = delete;
    AMRTrie& operator=(const AMRTrie&) = delete;

    void clear();

    // Insert an Antimicrobial Resistance marker into the Trie and Bloom Filter
    void insertMarker(const std::string& marker, 
                      const std::string& metadata = "", 
                      const std::string& targetDrug = "", 
                      const std::string& mechanism = "");

    // Search for an exact marker match (Hamming distance = 0) with Bloom Filter fast-rejection
    bool searchExact(const std::string& marker, std::string* outMetadata = nullptr) const;

    // Recursive search allowing up to maxMismatches (Hamming distance <= maxMismatches)
    bool searchWithMismatch(const std::string& marker, int maxMismatches = 1, std::string* outMetadata = nullptr) const;

    const AMRBloomFilter& getBloomFilter() const { return bloomFilter; }
    size_t getNodeCount() const { return arena.getNodeCount(); }
    size_t getMemoryUsage() const { return arena.getMemoryUsage(); }

private:
    TrieNode* root;
    mutable TrieArena arena;
    AMRBloomFilter bloomFilter;

    int charToIndex(char c) const;
    bool searchMismatchHelper(const TrieNode* node, const std::string& marker, size_t index, int mismatchesRemaining, std::string* outMetadata) const;
};

#endif // AMR_TRIE_H
