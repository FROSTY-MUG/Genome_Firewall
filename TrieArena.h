#ifndef TRIE_ARENA_H
#define TRIE_ARENA_H

#include <vector>
#include <cstddef>
#include <utility>

// Forward declaration
struct TrieNode;

// Arena Allocator for fast, cache-friendly allocation of TrieNodes
class TrieArena {
public:
    explicit TrieArena(size_t blockCapacity = 8192);
    ~TrieArena();

    TrieArena(const TrieArena&) = delete;
    TrieArena& operator=(const TrieArena&) = delete;

    // Allocate a TrieNode from the contiguous arena memory pool
    TrieNode* allocateNode();

    // Reset the arena, freeing all memory chunks
    void reset();

    // Return total nodes allocated across all blocks
    size_t getNodeCount() const { return totalNodesAllocated; }

    // Return total memory consumed in bytes
    size_t getMemoryUsage() const;

private:
    void allocateBlock();

    size_t blockCapacity;
    size_t currentBlockIndex;
    size_t totalNodesAllocated;
    std::vector<TrieNode*> blocks;
};

#endif // TRIE_ARENA_H
