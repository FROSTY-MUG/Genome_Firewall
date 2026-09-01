#include "ScoringEngine.h"
#include <iostream>

// ---------------------------------------------------------------------------
// Thread entry point — called by CreateThread / pthread_create
// ---------------------------------------------------------------------------

#ifdef _WIN32
DWORD WINAPI ScoringEngine::threadEntry(LPVOID param) {
    ChunkTask* task = static_cast<ChunkTask*>(param);
    task->engine->processSequenceChunk(task->sequence, task->kmerLength);
    delete task;
    return 0;
}
#else
void* ScoringEngine::threadEntry(void* param) {
    ChunkTask* task = static_cast<ChunkTask*>(param);
    task->engine->processSequenceChunk(task->sequence, task->kmerLength);
    delete task;
    return NULL;
}
#endif

// ---------------------------------------------------------------------------
// Constructor
// ---------------------------------------------------------------------------

ScoringEngine::ScoringEngine(const AMRTrie& trie, const AMRDatabase& db,
                             bool enableFuzzy, double confidenceThreshold)
    : amrTrie(trie),
      amrDb(db),
      fuzzyMatchEnabled(enableFuzzy),
      predictor(trie, confidenceThreshold) {
    size_t numRecords = amrDb.getRecords().size();
    if (numRecords == 0) numRecords = 10;
    featureVectorAtomics = std::vector<std::atomic<int>>(numRecords);
    for (auto& atom : featureVectorAtomics) {
        atom.store(0);
    }
}

// ---------------------------------------------------------------------------
// Sequential chunk processing
// ---------------------------------------------------------------------------

void ScoringEngine::processSequenceChunk(const std::string& sequence, size_t kmerLength) {
    if (sequence.length() < kmerLength) return;

    int localExact   = 0;
    int localMutated = 0;

    const auto& records = amrDb.getRecords();

    for (size_t i = 0; i <= sequence.length() - kmerLength; ++i) {
        std::string kmer = sequence.substr(i, kmerLength);

        std::string meta;
        if (amrTrie.searchExact(kmer, &meta)) {
            localExact++;

            // Map detected k-mer to index in Binary Feature Vector X
            for (size_t idx = 0; idx < records.size(); ++idx) {
                if (records[idx].sequence.find(kmer) != std::string::npos ||
                    (!meta.empty() && meta.find(records[idx].markerId) != std::string::npos)) {
                    featureVectorAtomics[idx].store(1);
                }
            }
        } else if (fuzzyMatchEnabled && amrTrie.searchWithMismatch(kmer, 1, &meta)) {
            localMutated++;

            for (size_t idx = 0; idx < records.size(); ++idx) {
                if (!meta.empty() && meta.find(records[idx].markerId) != std::string::npos) {
                    featureVectorAtomics[idx].store(1);
                }
            }
        }
    }

    predictor.analyzeSequenceFeatures(sequence, kmerLength, fuzzyMatchEnabled);

    if (localExact   > 0) exactMatches.fetch_add(localExact);
    if (localMutated > 0) mutatedMatches.fetch_add(localMutated);

    int localScore = localExact + localMutated;
    if (localScore > 0) {
        resistanceScore.fetch_add(localScore);
    }
}

// ---------------------------------------------------------------------------
// Async chunk dispatch — spawns a native OS thread per chunk
// ---------------------------------------------------------------------------

void ScoringEngine::processSequenceChunkAsync(const std::string& sequence, size_t kmerLength) {
    ChunkTask* task = new ChunkTask{this, sequence, kmerLength};

    WinLockGuard lock(tasksMutex);

#ifdef _WIN32
    AsyncTask at;
    at.hThread = CreateThread(NULL, 0, ScoringEngine::threadEntry, task, 0, NULL);
    if (at.hThread == NULL) {
        // Fallback to synchronous if thread creation fails
        delete task;
        processSequenceChunk(sequence, kmerLength);
        return;
    }
    asyncTasks.push_back(std::move(at));
#else
    AsyncTask at;
    int rc = pthread_create(&at.tid, NULL, ScoringEngine::threadEntry, task);
    if (rc != 0) {
        delete task;
        processSequenceChunk(sequence, kmerLength);
        return;
    }
    at.running = true;
    asyncTasks.push_back(std::move(at));
#endif
}

// ---------------------------------------------------------------------------
// Wait for all spawned threads to complete
// ---------------------------------------------------------------------------

void ScoringEngine::waitForAsyncTasks() {
    WinLockGuard lock(tasksMutex);
    for (auto& task : asyncTasks) {
        if (task.valid()) {
            task.get();
        }
    }
    asyncTasks.clear();
}

// ---------------------------------------------------------------------------
// Extract the binary feature vector (thread-safe atomic loads)
// ---------------------------------------------------------------------------

std::vector<int> ScoringEngine::getBinaryFeatureVector() const {
    std::vector<int> result(featureVectorAtomics.size());
    for (size_t i = 0; i < featureVectorAtomics.size(); ++i) {
        result[i] = featureVectorAtomics[i].load();
    }
    return result;
}
