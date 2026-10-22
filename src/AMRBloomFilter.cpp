#include "AMRBloomFilter.h"
#include <algorithm>

AMRBloomFilter::AMRBloomFilter(size_t bitSize, size_t hashCount)
    : words((bitSize + 63) / 64, 0ULL),
      totalBits(bitSize),
      hashCount(hashCount),
      isPowerOfTwo((bitSize & (bitSize - 1)) == 0),
      mask(bitSize - 1) {}

void AMRBloomFilter::add(const std::string& kmer) {
    add(kmer.data(), kmer.length());
}

void AMRBloomFilter::add(const char* data, size_t len) {
    if (len == 0 || totalBits == 0) return;

    uint64_t h1, h2, h3;
    computeHashes(data, len, h1, h2, h3);

    size_t idx1 = isPowerOfTwo ? (h1 & mask) : (h1 % totalBits);
    size_t idx2 = isPowerOfTwo ? (h2 & mask) : (h2 % totalBits);

    words[idx1 >> 6] |= (1ULL << (idx1 & 63));
    words[idx2 >> 6] |= (1ULL << (idx2 & 63));

    if (hashCount >= 3) {
        size_t idx3 = isPowerOfTwo ? (h3 & mask) : (h3 % totalBits);
        words[idx3 >> 6] |= (1ULL << (idx3 & 63));
    }
}

bool AMRBloomFilter::contains(const std::string& kmer) const {
    return contains(kmer.data(), kmer.length());
}

bool AMRBloomFilter::contains(const char* data, size_t len) const {
    if (len == 0 || totalBits == 0) return false;

    // Check first hash with early-exit short-circuit
    uint64_t h1 = hash1(data, len);
    size_t idx1 = isPowerOfTwo ? (h1 & mask) : (h1 % totalBits);
    if (!((words[idx1 >> 6] >> (idx1 & 63)) & 1ULL)) {
        return false;
    }

    // Check second hash only if first bit was set
    uint64_t h2 = hash2(data, len);
    size_t idx2 = isPowerOfTwo ? (h2 & mask) : (h2 % totalBits);
    if (!((words[idx2 >> 6] >> (idx2 & 63)) & 1ULL)) {
        return false;
    }

    // Check third hash only if first two bits were set
    if (hashCount >= 3) {
        uint64_t h3 = hash3(data, len);
        size_t idx3 = isPowerOfTwo ? (h3 & mask) : (h3 % totalBits);
        if (!((words[idx3 >> 6] >> (idx3 & 63)) & 1ULL)) {
            return false;
        }
    }

    return true; // Fast-path pass: candidate for exact Trie verification
}

void AMRBloomFilter::clear() {
    std::fill(words.begin(), words.end(), 0ULL);
}
