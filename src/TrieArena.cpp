#include "TrieArena.h"
#include "AMRTrie.h"
#include <new>
#include <cstring>
#include <iostream>

TrieArena::TrieArena(size_t blockCapacity)
    : blockCapacity(blockCapacity),
      currentBlockIndex(0),
      totalNodesAllocated(0),
      currentBlockPtr(nullptr) {
    allocateBlock();
}

TrieArena::~TrieArena() {
    for (TrieNode* block : blocks) {
        ::operator delete[](block);
    }
    blocks.clear();
    currentBlockPtr = nullptr;
}

void TrieArena::allocateBlock() {
    TrieNode* rawBlock = static_cast<TrieNode*>(::operator new[](blockCapacity * sizeof(TrieNode)));
    std::memset(rawBlock, 0, blockCapacity * sizeof(TrieNode));
    blocks.push_back(rawBlock);
    currentBlockPtr = rawBlock;
    currentBlockIndex = 0;
}

TrieNode* TrieArena::allocateNode() {
    if (__builtin_expect(currentBlockIndex >= blockCapacity, 0)) {
        allocateBlock();
    }

    TrieNode* nodePtr = &currentBlockPtr[currentBlockIndex++];
    nodePtr->markerRecordIndex = -1;
    totalNodesAllocated++;
    return nodePtr;
}

void TrieArena::reset() {
    for (TrieNode* block : blocks) {
        ::operator delete[](block);
    }
    blocks.clear();
    currentBlockPtr = nullptr;
    currentBlockIndex = 0;
    totalNodesAllocated = 0;
    allocateBlock();
}

size_t TrieArena::getMemoryUsage() const {
    return blocks.size() * blockCapacity * sizeof(TrieNode);
}
