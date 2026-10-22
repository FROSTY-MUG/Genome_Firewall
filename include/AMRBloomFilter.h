#ifndef AMR_BLOOM_FILTER_H
#define AMR_BLOOM_FILTER_H

#include <vector>
#include <string>
#include <cstdint>
#include <cstddef>
#include <cstring>

// High-performance Bit-Vector Bloom Filter for O(1) Fast-Path Rejection
class AMRBloomFilter {
public:
    explicit AMRBloomFilter(size_t bitSize = 1048576, size_t hashCount = 3);
    ~AMRBloomFilter() = default;

    void add(const std::string& kmer);
    void add(const char* data, size_t len);

    bool contains(const std::string& kmer) const;
    bool contains(const char* data, size_t len) const;

    void clear();

    size_t getBitSize() const { return totalBits; }

    static inline void computeHashes(const char* data, size_t len, uint64_t& h1, uint64_t& h2, uint64_t& h3) {
        uint64_t hash1 = 14695981039346656037ULL;
        uint64_t hash2 = 0xcbf29ce484222325ULL;
        uint64_t hash3 = 0;
        uint64_t p_pow = 1;

        for (size_t i = 0; i < len; ++i) {
            uint64_t c = static_cast<uint8_t>(data[i]);
            // hash1 (FNV-1a 64-bit)
            hash1 = (hash1 ^ c) * 1099511628211ULL;
            // hash2
            hash2 = (hash2 ^ c) * 0x100000001b3ULL;
            hash2 = (hash2 << 13) | (hash2 >> 51);
            // hash3
            hash3 += (c - 'A' + 1) * p_pow;
            p_pow *= 31;
        }
        h1 = hash1;
        h2 = hash2;
        h3 = hash3;
    }

    static inline uint64_t hash1(const std::string& kmer) {
        return hash1(kmer.data(), kmer.length());
    }
    static inline uint64_t hash1(const char* data, size_t len) {
        uint64_t hash = 14695981039346656037ULL;
        for (size_t i = 0; i < len; ++i) {
            hash ^= static_cast<uint64_t>(static_cast<uint8_t>(data[i]));
            hash *= 1099511628211ULL;
        }
        return hash;
    }

    static inline uint64_t hash2(const std::string& kmer) {
        return hash2(kmer.data(), kmer.length());
    }
    static inline uint64_t hash2(const char* data, size_t len) {
        uint64_t hash = 0xcbf29ce484222325ULL;
        for (size_t i = 0; i < len; ++i) {
            hash = (hash ^ static_cast<uint64_t>(static_cast<uint8_t>(data[i]))) * 0x100000001b3ULL;
            hash = (hash << 13) | (hash >> 51);
        }
        return hash;
    }

    static inline uint64_t hash3(const std::string& kmer) {
        return hash3(kmer.data(), kmer.length());
    }
    static inline uint64_t hash3(const char* data, size_t len) {
        uint64_t hash = 0;
        uint64_t p_pow = 1;
        for (size_t i = 0; i < len; ++i) {
            hash += (static_cast<uint64_t>(static_cast<uint8_t>(data[i])) - 'A' + 1) * p_pow;
            p_pow *= 31;
        }
        return hash;
    }

private:
    std::vector<uint64_t> words;
    size_t totalBits;
    size_t hashCount;
    bool isPowerOfTwo;
    size_t mask;
};

#endif // AMR_BLOOM_FILTER_H
