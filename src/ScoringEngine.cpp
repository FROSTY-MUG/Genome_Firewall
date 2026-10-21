#include "ScoringEngine.h"
#include <iostream>
#include <algorithm>

#ifdef _WIN32

DWORD WINAPI ScoringEngine::workerLoop(LPVOID param) {
    ScoringEngine* engine = static_cast<ScoringEngine*>(param);

    while (true) {
        WaitForSingleObject(engine->hWorkSemaphore, INFINITE);

        ChunkTask task;
        {
            WinLockGuard lock(engine->queueMutex);
            if (engine->stopPool && engine->taskQueue.empty()) {
                break;
            }
            if (engine->taskQueue.empty()) {
                continue;
            }
            task = std::move(engine->taskQueue.front());
            engine->taskQueue.pop_front();
        }

        engine->processSequenceChunk(task.sequence, task.kmerLength);

        if (engine->activeTasksCount.fetch_sub(1, std::memory_order_acq_rel) == 1) {
            SetEvent(engine->hDoneEvent);
        }
    }
    return 0;
}

void ScoringEngine::initThreadPool() {
    SYSTEM_INFO sysInfo;
    GetSystemInfo(&sysInfo);
    DWORD numWorkers = sysInfo.dwNumberOfProcessors;
    if (numWorkers < 2) numWorkers = 2;
    if (numWorkers > 16) numWorkers = 16;

    hWorkSemaphore = CreateSemaphore(NULL, 0, 100000, NULL);
    hDoneEvent = CreateEvent(NULL, TRUE, TRUE, NULL); // Initially signaled (no tasks active)

    workerHandles.reserve(numWorkers);
    for (DWORD i = 0; i < numWorkers; ++i) {
        HANDLE h = CreateThread(NULL, 0, ScoringEngine::workerLoop, this, 0, NULL);
        if (h != NULL) {
            workerHandles.push_back(h);
        }
    }
}

void ScoringEngine::shutdownThreadPool() {
    stopPool = true;
    for (size_t i = 0; i < workerHandles.size(); ++i) {
        ReleaseSemaphore(hWorkSemaphore, 1, NULL);
    }
    for (HANDLE h : workerHandles) {
        WaitForSingleObject(h, INFINITE);
        CloseHandle(h);
    }
    workerHandles.clear();

    if (hWorkSemaphore) {
        CloseHandle(hWorkSemaphore);
        hWorkSemaphore = NULL;
    }
    if (hDoneEvent) {
        CloseHandle(hDoneEvent);
        hDoneEvent = NULL;
    }
}

#else

void* ScoringEngine::workerLoop(void* param) {
    ScoringEngine* engine = static_cast<ScoringEngine*>(param);
    while (true) {
        pthread_mutex_lock(&engine->posixMutex);
        while (!engine->stopPool && engine->taskQueue.empty()) {
            pthread_cond_wait(&engine->posixWorkCond, &engine->posixMutex);
        }
        if (engine->stopPool && engine->taskQueue.empty()) {
            pthread_mutex_unlock(&engine->posixMutex);
            break;
        }
        ChunkTask task = std::move(engine->taskQueue.front());
        engine->taskQueue.pop_front();
        pthread_mutex_unlock(&engine->posixMutex);

        engine->processSequenceChunk(task.sequence, task.kmerLength);

        pthread_mutex_lock(&engine->posixMutex);
        if (engine->activeTasksCount.fetch_sub(1, std::memory_order_acq_rel) == 1) {
            pthread_cond_broadcast(&engine->posixDoneCond);
        }
        pthread_mutex_unlock(&engine->posixMutex);
    }
    return NULL;
}

void ScoringEngine::initThreadPool() {
    pthread_mutex_init(&posixMutex, NULL);
    pthread_cond_init(&posixWorkCond, NULL);
    pthread_cond_init(&posixDoneCond, NULL);

    size_t numWorkers = 4;
    workerThreads.resize(numWorkers);
    for (size_t i = 0; i < numWorkers; ++i) {
        pthread_create(&workerThreads[i], NULL, ScoringEngine::workerLoop, this);
    }
}

void ScoringEngine::shutdownThreadPool() {
    pthread_mutex_lock(&posixMutex);
    stopPool = true;
    pthread_cond_broadcast(&posixWorkCond);
    pthread_mutex_unlock(&posixMutex);

    for (pthread_t t : workerThreads) {
        pthread_join(t, NULL);
    }
    workerThreads.clear();
    pthread_cond_destroy(&posixWorkCond);
    pthread_cond_destroy(&posixDoneCond);
    pthread_mutex_destroy(&posixMutex);
}

#endif

// ---------------------------------------------------------------------------
// Constructor & Destructor
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
        atom.store(0, std::memory_order_relaxed);
    }

    initThreadPool();
}

ScoringEngine::~ScoringEngine() {
    shutdownThreadPool();
}

// ---------------------------------------------------------------------------
// High-Throughput Single-Pass Chunk Processing (Zero String Allocation)
// ---------------------------------------------------------------------------

void ScoringEngine::processSequenceChunk(const std::string& sequence, size_t kmerLength) {
    if (sequence.length() < kmerLength) return;

    int localExact   = 0;
    int localMutated = 0;

    const auto& records = amrDb.getRecords();
    const size_t numRecords = records.size();

    std::unordered_map<std::string, int> localDirectHits;
    std::unordered_map<std::string, int> localMutatedHits;
    std::unordered_map<std::string, bool> localTargetIntegrity;

    std::vector<uint8_t> localFeatures(numRecords, 0);

    const char* seqData = sequence.data();
    const size_t maxI = sequence.length() - kmerLength;

    for (size_t i = 0; i <= maxI; ++i) {
        int recordIdx = -1;

        // Zero-copy exact match with Bloom filter fast rejection
        if (amrTrie.searchExact(seqData + i, kmerLength, &recordIdx)) {
            localExact++;
            if (recordIdx >= 0 && static_cast<size_t>(recordIdx) < numRecords) {
                localFeatures[recordIdx] = 1;
                const auto& rec = records[recordIdx];
                if (rec.recordType == "TargetGene") {
                    localTargetIntegrity[rec.targetDrug] = true;
                } else {
                    localDirectHits[rec.targetDrug]++;
                }
            }
        } else if (fuzzyMatchEnabled && amrTrie.searchWithMismatch(seqData + i, kmerLength, 1, &recordIdx)) {
            localMutated++;
            if (recordIdx >= 0 && static_cast<size_t>(recordIdx) < numRecords) {
                localFeatures[recordIdx] = 1;
                const auto& rec = records[recordIdx];
                if (rec.recordType != "TargetGene") {
                    localMutatedHits[rec.targetDrug]++;
                }
            }
        }
    }

    if (localExact > 0) exactMatches.fetch_add(localExact, std::memory_order_relaxed);
    if (localMutated > 0) mutatedMatches.fetch_add(localMutated, std::memory_order_relaxed);

    int localScore = localExact + localMutated;
    if (localScore > 0) {
        resistanceScore.fetch_add(localScore, std::memory_order_relaxed);
    }

    // Flush thread-local hits to global atomics
    for (size_t r = 0; r < numRecords; ++r) {
        if (localFeatures[r]) {
            featureVectorAtomics[r].store(1, std::memory_order_relaxed);
        }
    }

    // Merge thread-local hits into predictor under mutex only if hits occurred
    if (!localDirectHits.empty() || !localMutatedHits.empty() || !localTargetIntegrity.empty()) {
        WinLockGuard lock(tasksMutex);
        predictor.mergeHits(localDirectHits, localMutatedHits, localTargetIntegrity, localExact, localMutated);
    }
}

// ---------------------------------------------------------------------------
// Async Chunk Dispatcher (Persistent Thread Pool Worker Queue)
// ---------------------------------------------------------------------------

void ScoringEngine::processSequenceChunkAsync(const std::string& sequence, size_t kmerLength) {
    activeTasksCount.fetch_add(1, std::memory_order_relaxed);

#ifdef _WIN32
    ResetEvent(hDoneEvent);
    {
        WinLockGuard lock(queueMutex);
        taskQueue.push_back({sequence, kmerLength});
    }
    ReleaseSemaphore(hWorkSemaphore, 1, NULL);
#else
    pthread_mutex_lock(&posixMutex);
    taskQueue.push_back({sequence, kmerLength});
    pthread_cond_signal(&posixWorkCond);
    pthread_mutex_unlock(&posixMutex);
#endif
}

void ScoringEngine::waitForAsyncTasks() {
#ifdef _WIN32
    WaitForSingleObject(hDoneEvent, INFINITE);
#else
    pthread_mutex_lock(&posixMutex);
    while (activeTasksCount.load(std::memory_order_acquire) > 0) {
        pthread_cond_wait(&posixDoneCond, &posixMutex);
    }
    pthread_mutex_unlock(&posixMutex);
#endif
}

std::vector<int> ScoringEngine::getBinaryFeatureVector() const {
    std::vector<int> result(featureVectorAtomics.size());
    for (size_t i = 0; i < featureVectorAtomics.size(); ++i) {
        result[i] = featureVectorAtomics[i].load(std::memory_order_relaxed);
    }
    return result;
}
