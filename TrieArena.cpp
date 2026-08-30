#include "TrieArena.h"
#include "AMRTrie.h"
#include <new>
#include <iostream>

TrieArena::TrieArena(size_t blockCapacity)
    : blockCapacity(blockCapacity), currentBlockIndex(0), totalNodesAllocated(0) {
    allocateBlock();
}

TrieArena::~TrieArena() {
    reset();
}

void TrieArena::allocateBlock() {
    TrieNode* rawBlock = static_cast<TrieNode*>(::operator new[](blockCapacity * sizeof(TrieNode)));
    blocks.push_back(rawBlock);
    currentBlockIndex = 0;
}

TrieNode* TrieArena::allocateNode() {
    if (currentBlockIndex >= blockCapacity) {
        allocateBlock();
    }

    TrieNode* block = blocks.back();
    TrieNode* nodePtr = &block[currentBlockIndex++];
    totalNodesAllocated++;

    // Construct in-place (Placement new)
    return ::new (static_cast<void*>(nodePtr)) TrieNode();
}

void TrieArena::reset() {
    for (TrieNode* block : blocks) {
        // Destroy elements
        ::operator delete[](block);
    }
    blocks.clear();
    currentBlockIndex = 0;
    totalNodesAllocated = 0;
}

size_t TrieArena::getMemoryUsage() const {
    return blocks.size() * blockCapacity * sizeof(TrieNode);
}
