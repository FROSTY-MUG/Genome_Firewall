#ifndef SCORING_ENGINE_H
#define SCORING_ENGINE_H

#include <string>
#include <atomic>
#include <vector>
#include "AMRTrie.h"
#include "PredictorEngine.h"
#include "AMRDatabase.h"

// ---------------------------------------------------------------------------
// Cross-platform lightweight task handle wrapper
// On Windows (MinGW 6.3) we use CreateThread instead of std::async/std::future
// because MinGW.org's GCC 6.3 ships a broken <future>/<mutex>/<thread>.
// ---------------------------------------------------------------------------

#ifdef _WIN32
#ifndef WIN32_LEAN_AND_MEAN
#define WIN32_LEAN_AND_MEAN
#endif
#include <windows.h>

struct AsyncTask {
    HANDLE hThread{NULL};
    bool valid() const { return hThread != NULL; }
    void get() {
        if (hThread) {
            WaitForSingleObject(hThread, INFINITE);
            CloseHandle(hThread);
            hThread = NULL;
        }
    }
};

// Win32 CRITICAL_SECTION wrapper (replaces std::mutex)
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
// POSIX fallback (Linux/macOS) — use pthreads directly
#include <pthread.h>

struct AsyncTask {
    pthread_t tid{0};
    bool running{false};
    bool valid() const { return running; }
    void get() {
        if (running) {
            pthread_join(tid, NULL);
            running = false;
        }
    }
};

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
    ~ScoringEngine() = default;

    ScoringEngine(const ScoringEngine&) = delete;
    ScoringEngine& operator=(const ScoringEngine&) = delete;

    void processSequenceChunk(const std::string& sequence, size_t kmerLength);
    void processSequenceChunkAsync(const std::string& sequence, size_t kmerLength);
    void waitForAsyncTasks();

    // Get the generated Binary Feature Vector X (1 = present, 0 = absent)
    std::vector<int> getBinaryFeatureVector() const;

    int getResistanceScore() const { return resistanceScore.load(); }
    int getExactHits()       const { return exactMatches.load(); }
    int getMutatedHits()     const { return mutatedMatches.load(); }

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

    std::vector<AsyncTask> asyncTasks;
    mutable WinMutex       tasksMutex;

    PredictorEngine predictor;

    // Thread entry struct for async dispatch
    struct ChunkTask {
        ScoringEngine* engine;
        std::string    sequence;
        size_t         kmerLength;
    };

#ifdef _WIN32
    static DWORD WINAPI threadEntry(LPVOID param);
#else
    static void* threadEntry(void* param);
#endif
};

#endif // SCORING_ENGINE_H
