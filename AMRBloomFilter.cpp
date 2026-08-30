#include "AMRBloomFilter.h"

AMRBloomFilter::AMRBloomFilter(size_t bitSize, size_t hashCount)
    : bits(bitSize, false), hashCount(hashCount) {}

void AMRBloomFilter::add(const std::string& kmer) {
    if (kmer.empty() || bits.empty()) return;

    uint64_t h1 = hash1(kmer) % bits.size();
    uint64_t h2 = hash2(kmer) % bits.size();
    bits[h1] = true;
    bits[h2] = true;

    if (hashCount >= 3) {
        uint64_t h3 = hash3(kmer) % bits.size();
        bits[h3] = true;
    }
}

bool AMRBloomFilter::contains(const std::string& kmer) const {
    if (kmer.empty() || bits.empty()) return false;

    uint64_t h1 = hash1(kmer) % bits.size();
    if (!bits[h1]) return false;

    uint64_t h2 = hash2(kmer) % bits.size();
    if (!bits[h2]) return false;

    if (hashCount >= 3) {
        uint64_t h3 = hash3(kmer) % bits.size();
        if (!bits[h3]) return false;
    }

    return true; // Might be a false positive, but never a false negative
}

void AMRBloomFilter::clear() {
    std::fill(bits.begin(), bits.end(), false);
}

// Hash 1: FNV-1a 64-bit
uint64_t AMRBloomFilter::hash1(const std::string& kmer) const {
    uint64_t hash = 14695981039346656037ULL;
    for (char c : kmer) {
        hash ^= static_cast<uint64_t>(c);
        hash *= 1099511628211ULL;
    }
    return hash;
}

// Hash 2: Murmur3-inspired 64-bit mixer
uint64_t AMRBloomFilter::hash2(const std::string& kmer) const {
    uint64_t hash = 0xcbf29ce484222325ULL;
    for (char c : kmer) {
        hash = (hash ^ static_cast<uint64_t>(c)) * 0x100000001b3ULL;
        hash = (hash << 13) | (hash >> 51);
    }
    return hash;
}

// Hash 3: Polynomial rolling hash
uint64_t AMRBloomFilter::hash3(const std::string& kmer) const {
    uint64_t hash = 0;
    uint64_t p = 31;
    uint64_t p_pow = 1;
    for (char c : kmer) {
        hash = (hash + (static_cast<uint64_t>(c) - 'A' + 1) * p_pow);
        p_pow = (p_pow * p);
    }
    return hash;
}
