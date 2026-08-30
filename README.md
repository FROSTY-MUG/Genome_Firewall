# GENOME FIREWALL (v2.0.0)
**Multithreaded High-Throughput Early-Warning Defense System Against Superbugs**

---

## 🏛️ Academic Context
**Submission:** 3rd-Semester Project-Based Learning (PBL)
**Program:** B.Tech CSE (AI & ML Honors) - Graphic Era University

**Engineering Philosophy:** 
While this project operates within the Artificial Intelligence and Bioinformatics domain (predictive decision support), the implementation strictly eschews high-level Python ML libraries. Instead, the "AI" is implemented from scratch using pure C/C++ systems-level programming. This project demonstrates maximum computational throughput, deterministic memory safety, concurrent multicore scaling, and core Computer Science fundamentals (Data Structures & OOP).

---

## 🧬 Project Overview
Genome Firewall is a high-performance terminal-based bioinformatics tool acting as an early-warning defense system against antibiotic-resistant bacteria. The system ingests raw bacterial DNA (`.fasta` files), cross-references substrings against an indexed database of Antimicrobial Resistance (AMR) markers with mutation tolerance, and deterministically predicts antibiotic efficacy.

---

## ⚙️ Architecture Pipeline

The system is modularized into three core engines, heavily leveraging Object-Oriented Programming (OOP) and systems-level C++11 concurrency:

1. **Sequence Ingestion Engine (`FastaParser`)**
   - Safely streams multi-gigabyte `.fasta` files using fixed-size buffers ($\mathcal{O}(1)$ auxiliary RAM).
   - Automatically strips metadata headers and whitespaces.
   - Implements an `overlapBuffer` to ensure resistance markers are never missed across chunk boundaries.

2. **Mutation-Tolerant Prefix Tree Engine (`AMRTrie`)**
   - Custom 4-ary Prefix Tree (Trie) tailored strictly to the genomic alphabet {A, C, G, T}.
   - Utilizes C++11 `std::unique_ptr` for strict, leak-free RAII memory management.
   - **Mutation-Tolerant Search:** Recursive backtracking algorithm supporting **Hamming Distance $\le 1$** (1-mismatch tolerance for Single Nucleotide Polymorphisms / SNPs).

3. **Multithreaded Scoring Engine (`ScoringEngine`)**
   - Dispatches sliding-window tasks concurrently across CPU cores using C++11 `std::async`.
   - **Zero Lock Contention on Lookups:** The `AMRTrie` is immutable during scoring, enabling 100% thread-safe concurrent reads without mutex locks.
   - **Lock-Free Atomic Counters:** Aggregates exact and mutated AMR hits using `std::atomic<int>` with `fetch_add`.
   - **High-Resolution Telemetry:** Profiles parsing and scoring execution down to the millisecond using `<chrono>`.
   - **Automated Report Export:** Generates formatted tabular ASCII reports and writes them to timestamped files (`report_YYYYMMDD_HHMMSS.txt`).

---

## 🚀 Build & Run Instructions

**Prerequisites:** 
- A UNIX-like environment (Linux/macOS/WSL)
- `g++` compiler (supporting C++11 or higher with POSIX threads)
- `make`

**1. Compile the Source Code:**
Navigate to the project directory and run the Makefile:
```bash
make
```
*This will generate the `genome_firewall` executable linking with `-pthread` and `-O2` optimizations.*

**2. Execute the Defense System:**
```bash
# Default execution (Multithreaded + Mutation-Tolerant):
./genome_firewall sample.fasta markers.csv

# Optional execution flags:
./genome_firewall sample.fasta markers.csv --fuzzy --async
./genome_firewall sample.fasta markers.csv --exact --serial
```

**3. Clean the Build:**
```bash
make clean
```

---

## 📊 Algorithmic Complexity (Big-O)

- **Exact Search Time Complexity:** $\mathcal{O}(K)$
  Following a single path down the 4-ary Trie is strictly bounded by $K$ (marker length, e.g. 21 bp). Completely independent of database size $N$.
- **Fuzzy Search Time Complexity (Hamming Distance = 1):** $\mathcal{O}(3 \cdot K^2)$ or $\mathcal{O}(K \cdot |\Sigma|)$
  At each of the $K$ positions, branching into $(|\Sigma|-1) = 3$ sibling nodes is explored. Once the single allowed mismatch is spent, all subsequent levels follow deterministic $\mathcal{O}(1)$ paths. Because $|\Sigma|=4$ is a small constant, this remains polynomial and completely decoupled from database size $N$.
- **Parallel Chunk Scoring:** $\mathcal{O}\left(\frac{M}{P}\right)$ where $M$ is total genome length and $P$ is the number of concurrent worker threads.
- **Space Complexity (Trie):** $\mathcal{O}(N \times K)$ worst case, with significant compression achieved via **Prefix Sharing** for homologous bacterial genes.
- **Auxiliary Memory:** $\mathcal{O}(1)$ streaming memory footprint.

---
*Developed from scratch in C++. Validated for 0 memory leaks via Valgrind.*

