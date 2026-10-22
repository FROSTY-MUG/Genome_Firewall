#ifndef AMR_TRIE_H
#define AMR_TRIE_H

#include <string>
#include <vector>
#include <cstddef>
#include <cstdint>
#include "TrieArena.h"
#include "AMRBloomFilter.h"

// Forward declaration
struct AMRRecord;

// ---------------------------------------------------------------------------
// Compact TrieNode (20 bytes on 32-bit, 40 bytes on 64-bit architecture)
// ---------------------------------------------------------------------------
struct TrieNode {
    TrieNode* children[4]{nullptr, nullptr, nullptr, nullptr};
    int markerRecordIndex{-1}; // -1 if non-terminal; >= 0 is index into AMRDatabase records
};

// Global branchless character-to-index lookup table in L1 cache
extern const int8_t CHAR_TO_INDEX[256];

class AMRTrie {
public:
    AMRTrie();
    ~AMRTrie() = default;

    AMRTrie(const AMRTrie&) = delete;
    AMRTrie& operator=(const AMRTrie&) = delete;

    void clear();

    // Insert an Antimicrobial Resistance marker k-mer into the Trie and Bloom Filter
    void insertMarker(const std::string& marker, int recordIndex);
    void insertMarker(const char* marker, size_t length, int recordIndex);

    // Fast exact marker search (Bloom filter pre-rejection + direct trie descent)
    bool searchExact(const std::string& marker, int* outRecordIndex = nullptr) const;
    bool searchExact(const char* marker, size_t length, int* outRecordIndex = nullptr) const;

    // Mutation-tolerant search (Hamming distance <= maxMismatches)
    bool searchWithMismatch(const std::string& marker, int maxMismatches = 1, int* outRecordIndex = nullptr) const;
    bool searchWithMismatch(const char* marker, size_t length, int maxMismatches = 1, int* outRecordIndex = nullptr) const;

    // String metadata compatibility overloads for legacy/REPL callers
    bool searchExact(const std::string& marker, std::string* outMetadata) const;
    bool searchWithMismatch(const std::string& marker, int maxMismatches, std::string* outMetadata) const;

    const AMRBloomFilter& getBloomFilter() const { return bloomFilter; }
    size_t getNodeCount() const { return arena.getNodeCount(); }
    size_t getMemoryUsage() const { return arena.getMemoryUsage(); }

    // Fast inline nucleotide character to index mapper (A=0, C=1, G=2, T=3)
    static inline int charToIndex(char c) {
        return CHAR_TO_INDEX[static_cast<uint8_t>(c)];
    }

private:
    TrieNode* root;
    mutable TrieArena arena;
    AMRBloomFilter bloomFilter;

    bool searchMismatchHelper(const TrieNode* node, const char* marker, size_t index, size_t length, int mismatchesRemaining, int* outRecordIndex) const;
};

#endif // AMR_TRIE_H
