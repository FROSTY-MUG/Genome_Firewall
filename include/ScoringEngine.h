#ifndef SCORING_ENGINE_H
#define SCORING_ENGINE_H

#include <string>
#include <atomic>
#include <vector>
#include <deque>
#include "AMRTrie.h"
#include "PredictorEngine.h"
#include "AMRDatabase.h"

#ifdef _WIN32
#ifndef WIN32_LEAN_AND_MEAN
#define WIN32_LEAN_AND_MEAN
#endif
#include <windows.h>

// Win32 CRITICAL_SECTION wrapper
struct WinMutex {
    CRITICAL_SECTION cs;
    WinMutex()  { InitializeCriticalSection(&cs); }
    ~WinMutex() { DeleteCriticalSection(&cs); }
    void lock()   { EnterCriticalSection(&cs); }
    void unlock() { LeaveCriticalSection(&cs); }
};

struct WinLockGuard {
    WinMutex& m;
    explicit WinLockGuard(WinMutex& mx) : m(mx) { m.lock(); }
    ~WinLockGuard() { m.unlock(); }
};

#else
// POSIX fallback (Linux/macOS)
#include <pthread.h>

struct WinMutex {
    pthread_mutex_t m;
    WinMutex()  { pthread_mutex_init(&m, NULL); }
    ~WinMutex() { pthread_mutex_destroy(&m); }
    void lock()   { pthread_mutex_lock(&m); }
    void unlock() { pthread_mutex_unlock(&m); }
};

struct WinLockGuard {
    WinMutex& mx;
    explicit WinLockGuard(WinMutex& m) : mx(m) { mx.lock(); }
    ~WinLockGuard() { mx.unlock(); }
};

#endif // _WIN32

class ScoringEngine {
public:
    explicit ScoringEngine(const AMRTrie& trie, const AMRDatabase& db,
                           bool enableFuzzy = true, double confidenceThreshold = 75.0);
    ~ScoringEngine();

    ScoringEngine(const ScoringEngine&) = delete;
    ScoringEngine& operator=(const ScoringEngine&) = delete;

    void processSequenceChunk(const std::string& sequence, size_t kmerLength);
    void processSequenceChunkAsync(const std::string& sequence, size_t kmerLength);
    void waitForAsyncTasks();

    // Get generated Binary Feature Vector X (1 = present, 0 = absent)
    std::vector<int> getBinaryFeatureVector() const;

    int getResistanceScore() const { return resistanceScore.load(std::memory_order_relaxed); }
    int getExactHits()       const { return exactMatches.load(std::memory_order_relaxed); }
    int getMutatedHits()     const { return mutatedMatches.load(std::memory_order_relaxed); }

    PredictorEngine& getPredictor()             { return predictor; }
    const PredictorEngine& getPredictor() const { return predictor; }

private:
    const AMRTrie&      amrTrie;
    const AMRDatabase&  amrDb;
    bool                fuzzyMatchEnabled;

    std::atomic<int> resistanceScore{0};
    std::atomic<int> exactMatches{0};
    std::atomic<int> mutatedMatches{0};

    // Thread-safe Binary Feature Vector X
    std::vector<std::atomic<int>> featureVectorAtomics;

    mutable WinMutex tasksMutex;
    PredictorEngine  predictor;

    // Task queue struct for persistent worker thread pool
    struct ChunkTask {
        std::string sequence;
        size_t      kmerLength;
    };

    std::deque<ChunkTask> taskQueue;
    mutable WinMutex      queueMutex;
    std::atomic<int>      activeTasksCount{0};
    bool                  stopPool{false};

#ifdef _WIN32
    HANDLE                hWorkSemaphore{NULL};
    HANDLE                hDoneEvent{NULL};
    std::vector<HANDLE>   workerHandles;
    static DWORD WINAPI   workerLoop(LPVOID param);
#else
    pthread_mutex_t       posixMutex;
    pthread_cond_t        posixWorkCond;
    pthread_cond_t        posixDoneCond;
    std::vector<pthread_t> workerThreads;
    static void*          workerLoop(void* param);
#endif

    void initThreadPool();
    void shutdownThreadPool();
};

#endif // SCORING_ENGINE_H
