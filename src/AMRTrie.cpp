#include "AMRTrie.h"
#include <iostream>

// Branchless 256-byte lookup table mapping ASCII chars to Trie indices
// 'A'/'a' -> 0, 'C'/'c' -> 1, 'G'/'g' -> 2, 'T'/'t' -> 3, all other chars -> -1
static constexpr int8_t initCharToIndex(uint8_t c) {
    return (c == 'A' || c == 'a') ? 0 :
           (c == 'C' || c == 'c') ? 1 :
           (c == 'G' || c == 'g') ? 2 :
           (c == 'T' || c == 't') ? 3 : -1;
}

#define ROW16(base) \
    initCharToIndex(base+0),  initCharToIndex(base+1),  initCharToIndex(base+2),  initCharToIndex(base+3),  \
    initCharToIndex(base+4),  initCharToIndex(base+5),  initCharToIndex(base+6),  initCharToIndex(base+7),  \
    initCharToIndex(base+8),  initCharToIndex(base+9),  initCharToIndex(base+10), initCharToIndex(base+11), \
    initCharToIndex(base+12), initCharToIndex(base+13), initCharToIndex(base+14), initCharToIndex(base+15)

const int8_t CHAR_TO_INDEX[256] = {
    ROW16(0),   ROW16(16),  ROW16(32),  ROW16(48),
    ROW16(64),  ROW16(80),  ROW16(96),  ROW16(112),
    ROW16(128), ROW16(144), ROW16(160), ROW16(176),
    ROW16(192), ROW16(208), ROW16(224), ROW16(240)
};
#undef ROW16

AMRTrie::AMRTrie() : bloomFilter(1048576, 3) {
    root = arena.allocateNode();
}

void AMRTrie::clear() {
    arena.reset();
    bloomFilter.clear();
    root = arena.allocateNode();
}

void AMRTrie::insertMarker(const std::string& marker, int recordIndex) {
    insertMarker(marker.data(), marker.length(), recordIndex);
}

void AMRTrie::insertMarker(const char* marker, size_t length, int recordIndex) {
    if (length == 0) return;

    // Fast O(1) Bloom Filter addition
    bloomFilter.add(marker, length);

    TrieNode* current = root;
    for (size_t i = 0; i < length; ++i) {
        int idx = CHAR_TO_INDEX[static_cast<uint8_t>(marker[i])];
        if (idx < 0) return; // Skip invalid nucleotides

        if (!current->children[idx]) {
            current->children[idx] = arena.allocateNode();
        }
        current = current->children[idx];
    }
    current->markerRecordIndex = recordIndex;
}

bool AMRTrie::searchExact(const std::string& marker, int* outRecordIndex) const {
    return searchExact(marker.data(), marker.length(), outRecordIndex);
}

bool AMRTrie::searchExact(const char* marker, size_t length, int* outRecordIndex) const {
    // Fast O(1) Bloom Filter rejection check with short circuit
    if (!bloomFilter.contains(marker, length)) {
        return false;
    }

    const TrieNode* current = root;
    for (size_t i = 0; i < length; ++i) {
        int idx = CHAR_TO_INDEX[static_cast<uint8_t>(marker[i])];
        if (idx < 0) return false;

        current = current->children[idx];
        if (!current) return false;
    }

    if (current && current->markerRecordIndex >= 0) {
        if (outRecordIndex) *outRecordIndex = current->markerRecordIndex;
        return true;
    }
    return false;
}

bool AMRTrie::searchWithMismatch(const std::string& marker, int maxMismatches, int* outRecordIndex) const {
    return searchWithMismatch(marker.data(), marker.length(), maxMismatches, outRecordIndex);
}

bool AMRTrie::searchWithMismatch(const char* marker, size_t length, int maxMismatches, int* outRecordIndex) const {
    if (length == 0 || !root) return false;
    return searchMismatchHelper(root, marker, 0, length, maxMismatches, outRecordIndex);
}

bool AMRTrie::searchMismatchHelper(const TrieNode* node, const char* marker, size_t index, size_t length, int mismatchesRemaining, int* outRecordIndex) const {
    // When no mismatches remain, run tight iterative loop without recursive call overhead
    if (__builtin_expect(mismatchesRemaining == 0, 0)) {
        while (index < length) {
            int idx = CHAR_TO_INDEX[static_cast<uint8_t>(marker[index++])];
            if (idx < 0 || !(node = node->children[idx])) return false;
        }
        if (node->markerRecordIndex >= 0) {
            if (outRecordIndex) *outRecordIndex = node->markerRecordIndex;
            return true;
        }
        return false;
    }

    if (index == length) {
        if (node->markerRecordIndex >= 0) {
            if (outRecordIndex) *outRecordIndex = node->markerRecordIndex;
            return true;
        }
        return false;
    }

    char c = marker[index];
    int exactIdx = CHAR_TO_INDEX[static_cast<uint8_t>(c)];

    // Exact match branch first (highest branch probability)
    if (exactIdx >= 0 && node->children[exactIdx]) {
        if (searchMismatchHelper(node->children[exactIdx], marker, index + 1, length, mismatchesRemaining, outRecordIndex)) {
            return true;
        }
    }

    // Mismatch branches
    for (int i = 0; i < 4; ++i) {
        if (i != exactIdx && node->children[i]) {
            if (searchMismatchHelper(node->children[i], marker, index + 1, length, mismatchesRemaining - 1, outRecordIndex)) {
                return true;
            }
        }
    }

    return false;
}

// Legacy string overloads
bool AMRTrie::searchExact(const std::string& marker, std::string* outMetadata) const {
    int recIdx = -1;
    bool found = searchExact(marker, &recIdx);
    if (found && outMetadata) {
        *outMetadata = std::to_string(recIdx);
    }
    return found;
}

bool AMRTrie::searchWithMismatch(const std::string& marker, int maxMismatches, std::string* outMetadata) const {
    int recIdx = -1;
    bool found = searchWithMismatch(marker, maxMismatches, &recIdx);
    if (found && outMetadata) {
        *outMetadata = std::to_string(recIdx);
    }
    return found;
}
