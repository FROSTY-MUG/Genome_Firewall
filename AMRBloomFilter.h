#ifndef AMR_BLOOM_FILTER_H
#define AMR_BLOOM_FILTER_H

#include <vector>
#include <string>
#include <cstdint>

// High-performance Bit-Vector Bloom Filter for O(1) Fast-Path Rejection
class AMRBloomFilter {
public:
    explicit AMRBloomFilter(size_t bitSize = 1048576, size_t hashCount = 3);
    ~AMRBloomFilter() = default;

    void add(const std::string& kmer);
    bool contains(const std::string& kmer) const;
    void clear();

    size_t getBitSize() const { return bits.size(); }

private:
    uint64_t hash1(const std::string& kmer) const;
    uint64_t hash2(const std::string& kmer) const;
    uint64_t hash3(const std::string& kmer) const;

    std::vector<bool> bits;
    size_t hashCount;
};

#endif // AMR_BLOOM_FILTER_H
