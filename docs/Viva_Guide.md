# GENOME FIREWALL: PBL Concept Mapping & Viva Guide (v2.0 Advanced Systems Edition)

## PART 1: Data Structures Analysis (The Core Engine)

### 1. The 4-Ary Trie (Prefix Tree) Structure
If a professor asks you to whiteboard your data structure, explain that you rejected standard text structures in favor of domain-specific engineering. 
* **The Structure:** A standard Trie allocates an array of 256 pointers per node (for all ASCII characters), which wastes massive amounts of RAM. Because our domain is strictly Genomic DNA, we engineered a **4-ary Trie**. Each `TrieNode` contains exactly 4 pointers mapped strictly to {A, C, G, T}, plus a boolean `isEndOfMarker` flag.
* **Traversal:** We map characters to indices (A=0, C=1, G=2, T=3). Searching involves following the array index. If the pointer is null, the sequence doesn't exist. If we hit the end of our search string and `isEndOfMarker` is true, we have a match.

### 2. Mutation Tolerance: Recursive Hamming Distance Search
DNA undergoes point mutations (Single Nucleotide Polymorphisms / SNPs). Exact matching is too brittle for biological variants.
* **Algorithm:** We implemented a recursive backtracking algorithm in `searchMismatchHelper(node, marker, index, mismatchesRemaining)`.
* **Branching Logic:**
  1. *Branch 1 (Exact):* Traverses the direct matching child with $0$ penalty (`mismatchesRemaining`).
  2. *Branch 2 (Mutation):* If `mismatchesRemaining > 0`, it branches into the other 3 alternative nucleotide children, decrementing the budget (`mismatchesRemaining - 1`).
* **Complexity Impact:**
  * Exact Search: $\mathcal{O}(K)$ time for length $K$. Exactly 1 path is traversed.
  * Hamming Distance = 1 Search: At each of the $K$ positions, we can take at most 1 deviation into 3 sibling branches ($|\Sigma| - 1 = 3$). Once a deviation is taken, remaining mismatches is $0$, forcing the rest of that branch to be strictly deterministic ($1$ path).
  * Worst-case time is $\mathcal{O}(3 \cdot K^2)$ or $\mathcal{O}(K \cdot |\Sigma|)$. Because $|\Sigma| = 4$ is a constant, this remains strictly polynomial $\mathcal{O}(K^2)$, completely independent of the database size $N$!

### 3. Modern C++ vs. C-Style Memory Management
* **The C Approach (Raw Pointers):** In pure C, you would define `struct TrieNode* children[4]`. You would have to use `malloc()` for every node. Crucially, to prevent memory leaks, you would have to write a custom recursive post-order traversal function, calling `free()` on every single node at the end of the program. If a runtime error happened midway, your program would leak gigabytes of RAM.
* **Our C++ Approach (`std::unique_ptr`):** We used modern C++ smart pointers (`std::unique_ptr<TrieNode> children[4]`). This enforces strict single-ownership. When the tree is destroyed, the smart pointers automatically trigger a cascading, bottom-up deallocation of all child nodes. No manual `free()` loops, no dangling pointers, and 100% memory safety (proven by a clean Valgrind run).

---

## PART 2: Object-Oriented Programming & Systems Architecture

### 1. Encapsulation (Data Hiding)
* **Where we used it:** The `FastaParser` class.
* **Defense:** Encapsulation hides internal buffer state and file descriptors. The caller (`main()`) has no idea *how* the DNA is being read or chunked. The `std::ifstream` and `overlapBuffer` string are `private` members. `main()` simply calls `getNextChunk()`.

### 2. RAII (Resource Acquisition Is Initialization)
* **Where we used it:** System-wide (File I/O, Smart Pointers, and Mutex Locks).
* **Defense:** RAII binds the lifecycle of a system resource to the lifecycle of a stack-allocated object. When `std::lock_guard<std::mutex>` is instantiated, it acquires the lock; upon exiting the scope, it automatically releases it. If an exception occurs, stack unwinding closes all file handles and cascades pointer destruction safely.

### 3. Concurrency & Lock-Free Synchronization
* **Architecture:** The `AMRTrie` is **read-only** during the scoring phase. Multiple worker threads spawned via C++11 `std::async` can read from the same Trie concurrently with **zero mutex locks** (zero lock contention).
* **Atomic Telemetry:** The shared `resistanceScore`, `exactMatches`, and `mutatedMatches` variables are declared as `std::atomic<int>`. Worker threads perform `fetch_add` with `std::memory_order_relaxed`, which hardware-translates to single-instruction atomic assembly (`LOCK XADD` on x86), preventing data races without thread blocking.

### 4. High-Resolution Benchmarking & Persistence
* **`<chrono>` Profiling:** We utilize `std::chrono::high_resolution_clock` to benchmark chunk parsing and scoring phases with microsecond precision, calculating throughput in MB/s.
* **Disk Export:** The report is formatted using `<iomanip>` and persisted to a timestamped file (`report_YYYYMMDD_HHMMSS.txt`) using `std::ofstream`.

---

## PART 3: The Mock Viva Simulator (7 Toughest Questions)

**Q1: "You are processing the genome in fixed 4096-byte chunks. What happens if a 21-mer marker starts at byte 4086? Doesn't it get split across two chunks, causing your Trie to miss it?"**
* **High-Scoring Answer:** "That is a classic boundary condition. To solve it, I encapsulated an `overlapBuffer` inside `FastaParser`. When a chunk finishes reading, the engine extracts the last $K-1$ bytes (where $K$ is the marker length) and stores them in the buffer. When the next chunk is read, the buffer is prepended. This guarantees that any marker straddling a chunk boundary is successfully stitched back together in the sliding window."

**Q2: "What is the Big-O time complexity tradeoff when adding a Hamming Distance of 1 to your 4-ary Trie search compared to an exact search?"**
* **High-Scoring Answer:** "An exact Trie search is strictly $\mathcal{O}(K)$ where $K$ is marker length, following a single linear path. With Hamming Distance = 1, at each of the $K$ steps we can branch into up to $(|\Sigma| - 1) = 3$ alternate nucleotide children. Once a mismatch branch is taken, the remaining $K - \text{index}$ steps become strictly deterministic (0 mismatches remaining). Therefore, the search complexity scales to $\mathcal{O}(3 \cdot K^2)$ in the worst case. Crucially, because $|\Sigma|=4$ is small and fixed, this search remains completely independent of $N$ (the number of indexed markers in the database)."

**Q3: "In your multithreaded ScoringEngine, multiple threads query the same AMRTrie at the same time. Why don't you have a mutex protecting the Trie nodes?"**
* **High-Scoring Answer:** "Because the Trie is an **immutable, read-only data structure** during the scoring phase. All write operations occur during the initial database ingestion in single-threaded setup. In systems programming, concurrent reads to immutable memory are inherently thread-safe and free of data races. Adding a mutex around Trie lookups would create massive lock contention across CPU cores, completely defeating the purpose of multithreading."

**Q4: "Why did you use `std::atomic<int>::fetch_add` instead of wrapping standard integer increments inside a `std::mutex` lock?"**
* **High-Scoring Answer:** "A `std::mutex` involves OS-level context switching, kernel transitions, and thread suspension if contention occurs. `std::atomic<int>` uses CPU-level atomic primitives (like the `LOCK XADD` instruction on x86_64). Furthermore, our worker threads accumulate counts locally per chunk and execute only one `fetch_add` at the end of the chunk, eliminating false sharing, reducing bus traffic, and achieving near-linear multi-core scaling."

**Q5: "If your program throws a `std::runtime_error` because the `.fasta` file is missing, how do you guarantee the `AMRTrie` nodes are freed? I don't see a `delete` call in your catch block."**
* **High-Scoring Answer:** "This is guaranteed by C++ Stack Unwinding and RAII. Because `AMRTrie` is allocated on the stack in `main()`, when the exception is thrown, the runtime unwinds the stack and invokes the `AMRTrie` destructor before the catch block executes. Inside the Trie, `std::unique_ptr` smart pointers automatically trigger a cascading, bottom-up deallocation. Manual `delete` calls are anti-patterns in modern C++."

**Q6: "Why did you pass the `AMRTrie` to the `ScoringEngine` by `const reference` (`const AMRTrie&`) instead of by value?"**
* **High-Scoring Answer:** "Passing a massive data structure by value would force the compiler to deep-copy the entire tree structure, consuming gigabytes of RAM and ruining performance. Passing by reference avoids the copy. Applying the `const` qualifier enforces compile-time const-correctness, guaranteeing that worker threads and the scoring engine cannot mutate or invalidate the Trie."

**Q7: "DNA sequencing often outputs an 'N' for an unknown nucleotide. How does your 4-ary Trie handle invalid characters without segfaulting?"**
* **High-Scoring Answer:** "I implemented a `charToIndex()` helper method. If it encounters 'N' or any invalid character, it returns `-1`. Both the insertion and search loops explicitly check for this `-1` return value. In the search loop, an invalid character aborts that specific mismatch branch immediately, preventing out-of-bounds array indexing and guaranteeing memory safety."

