#include "AMRTrie.h"
#include <iostream>

AMRTrie::AMRTrie() : bloomFilter(1048576, 3) {
    root = arena.allocateNode();
}

void AMRTrie::clear() {
    arena.reset();
    bloomFilter.clear();
    root = arena.allocateNode();
}

int AMRTrie::charToIndex(char c) const {
    switch (c) {
        case 'A': return 0;
        case 'C': return 1;
        case 'G': return 2;
        case 'T': return 3;
        default: return -1;
    }
}

void AMRTrie::insertMarker(const std::string& marker, const std::string& metadata, const std::string& targetDrug, const std::string& mechanism) {
    if (marker.empty()) return;

    // Add marker to Bloom Filter for O(1) rejection
    bloomFilter.add(marker);

    TrieNode* current = root;
    for (char c : marker) {
        int idx = charToIndex(c);
        if (idx == -1) continue;

        if (!current->children[idx]) {
            current->children[idx] = arena.allocateNode();
        }
        current = current->children[idx];
    }
    current->isEndOfMarker = true;
    current->markerMetadata = metadata;
    current->targetDrug = targetDrug;
    current->mechanism = mechanism;
}

bool AMRTrie::searchExact(const std::string& marker, std::string* outMetadata) const {
    // Fast-path O(1) Bloom Filter rejection check
    if (!bloomFilter.contains(marker)) {
        return false;
    }

    TrieNode* current = root;
    for (char c : marker) {
        int idx = charToIndex(c);
        if (idx == -1) return false;

        if (!current->children[idx]) {
            return false;
        }
        current = current->children[idx];
    }

    if (current && current->isEndOfMarker) {
        if (outMetadata) *outMetadata = current->markerMetadata;
        return true;
    }
    return false;
}

bool AMRTrie::searchWithMismatch(const std::string& marker, int maxMismatches, std::string* outMetadata) const {
    if (marker.empty() || !root) return false;
    return searchMismatchHelper(root, marker, 0, maxMismatches, outMetadata);
}

bool AMRTrie::searchMismatchHelper(const TrieNode* node, const std::string& marker, size_t index, int mismatchesRemaining, std::string* outMetadata) const {
    if (!node) return false;

    if (index == marker.length()) {
        if (node->isEndOfMarker) {
            if (outMetadata) *outMetadata = node->markerMetadata;
            return true;
        }
        return false;
    }

    char c = marker[index];
    int exactIdx = charToIndex(c);

    // Exact match branch
    if (exactIdx != -1 && node->children[exactIdx]) {
        if (searchMismatchHelper(node->children[exactIdx], marker, index + 1, mismatchesRemaining, outMetadata)) {
            return true;
        }
    }

    // Mismatch branch
    if (mismatchesRemaining > 0) {
        for (int i = 0; i < 4; ++i) {
            if (i != exactIdx && node->children[i]) {
                if (searchMismatchHelper(node->children[i], marker, index + 1, mismatchesRemaining - 1, outMetadata)) {
                    return true;
                }
            }
        }
    }

    return false;
}
