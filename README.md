# GENOME FIREWALL (v2.0.0)
**Multithreaded High-Throughput Early-Warning Defense System Against Superbugs**

[![Build](https://img.shields.io/badge/build-GCC%206.3%20MinGW-success)](.) [![Language](https://img.shields.io/badge/language-C%2B%2B14-blue)](.) [![Standard](https://img.shields.io/badge/std-c%2B%2B14-informational)](.) [![Platform](https://img.shields.io/badge/platform-Windows%20%7C%20Linux%20%7C%20macOS-lightgrey)](.)

---

## 🏛️ Academic Context
**Submission:** 3rd-Semester Project-Based Learning (PBL)
**Program:** B.Tech CSE (AI & ML Honors) - Graphic Era University

**Engineering Philosophy:**
While this project operates within the Artificial Intelligence and Bioinformatics domain (predictive decision support), the implementation strictly eschews high-level Python ML libraries. Instead, the "AI" is implemented from scratch using pure C/C++ systems-level programming. This project demonstrates maximum computational throughput, deterministic memory safety, concurrent multicore scaling, and core Computer Science fundamentals (Data Structures & OOP).

---

## 🧬 Project Overview
Genome Firewall is a high-performance terminal-based bioinformatics tool acting as an early-warning defense system against antibiotic-resistant bacteria. The system ingests raw bacterial DNA (`.fasta` files), cross-references substrings against an indexed database of Antimicrobial Resistance (AMR) markers with mutation tolerance, and deterministically predicts antibiotic efficacy using an embedded Logistic Regression engine.

---

## 📦 Datasets Used

### AMR Reference Databases
The marker databases ship with two tiers:

| File | Markers | Drug Classes | Source |
|------|---------|-------------|--------|
| `markers.csv` | 10 curated | 5 (Amoxicillin, Tetracycline, Erythromycin, Gentamicin, Chloramphenicol) | NCBI/CARD |
| `markers_large.csv` | 53 curated | 10 drug classes | NCBI/CARD/ResFinder |

**Drug classes covered (markers_large.csv):**
- 🔵 **Beta-lactams / Amoxicillin** — blaTEM-1b, blaSHV-1/11/12, blaOXA-1/10/30
- 🟡 **Tetracycline / Doxycycline** — tetA/B/C/D/E/M/O/Q
- 🟠 **Macrolides / Erythromycin** — ermA, ermB, ermC, mphA
- 🟢 **Aminoglycosides / Gentamicin** — aac3-Ia/IIa, aac6-Ib, aph3-Ia, ant2-Ia
- 🔴 **Phenicols / Chloramphenicol** — catA1/A2, catB3
- ⚫ **Carbapenems / Meropenem** — blaKPC-1/2, blaNDM-1/5, blaOXA-48, blaIMP-1, blaVIM-1
- 🟣 **Fluoroquinolones / Ciprofloxacin** — gyrA-D87G, parC-S80I, qnrA1/B1/S1, aac6-Ib-cr
- 🩵 **Glycopeptides / Vancomycin** — vanA/B/C/D/E/G
- 🔶 **Rifamycins / Rifampin** — rpoB_S531L, rpoB_D516V, rpoB-RRDR

### Test Genome
| File | Description |
|------|-------------|
| `sample.fasta` | Basic E. coli K12 test genome |
| `sample_real.fasta` | **Multidrug-resistant E. coli K88 demo** — embeds 14 real AMR gene fragments (blaTEM-1b, blaOXA-1, tetM, ermB, aac6-Ib, catA1, blaKPC-2, blaNDM-1, gyrA-D87G, qnrB1, vanA, vanB, rpoB-RRDR, tetA) within randomized chromosomal flanking sequences, triggering both exact and fuzzy (Hamming ≤ 1 SNP) matches |

---

## ⚙️ Architecture Pipeline

The system is modularized into three core engines, heavily leveraging Object-Oriented Programming (OOP) and systems-level C++11/14 concurrency:

1. **Sequence Ingestion Engine (`FastaMmapParser`)**
   - Zero-copy streaming via Windows `CreateFileMapping` / POSIX `mmap`
   - Safely streams multi-gigabyte `.fasta` files with $\mathcal{O}(1)$ auxiliary RAM
   - Automatic metadata header stripping and whitespace normalization
   - `overlapBuffer` ensures resistance markers are never missed across chunk boundaries
   - Graceful fallback to `std::ifstream` if mmap fails

2. **Mutation-Tolerant Prefix Tree Engine (`AMRTrie` + `AMRBloomFilter`)**
   - Custom 4-ary Prefix Tree (Trie) tailored strictly to the genomic alphabet {A, C, G, T}
   - C++11 `std::unique_ptr` with custom Arena allocator for leak-free RAII memory management
   - **Fast-rejection Bloom Filter:** 1 MB bit-array with 3 hash functions for O(1) pre-screening
   - **Mutation-Tolerant Search:** Recursive backtracking algorithm supporting **Hamming Distance ≤ 1** (1-mismatch tolerance for Single Nucleotide Polymorphisms / SNPs)

3. **Multithreaded Scoring Engine (`ScoringEngine` + `PredictorEngine`)**
   - Dispatches sliding-window 21-mer tasks concurrently across CPU cores using `std::async`
   - **Zero Lock Contention on Lookups:** Immutable `AMRTrie` during scoring → 100% thread-safe concurrent reads
   - **Lock-Free Atomic Counters:** `std::atomic<int>` with `fetch_add` for exact and mutated AMR hits
   - **Target-Gated Binary Feature Vector X:** Maps k-mer hits to gene index positions
   - **Logistic Regression Predictor:** `P = σ(W·X + b)` per-drug with pre-calibrated weights
   - **High-Resolution Telemetry:** `<chrono>` profiling down to microsecond precision
   - **Automated Report Export:** Formatted ASCII decision tables with throughput metrics

---

## 🚀 Build & Run Instructions

### Prerequisites
**Windows (MinGW):**
```powershell
# GCC 6.3+ MinGW required. Verify:
g++ --version
mingw32-make --version
```

**Linux / macOS:**
```bash
# GCC 7+ or Clang 5+
g++ --version
make --version
```

### 1. Compile the Source Code
```powershell
# Windows (MinGW):
mingw32-make clean
mingw32-make

# Linux / macOS:
make clean
make
```
*Generates `genome_firewall` (or `genome_firewall.exe`) linking with `-pthread -O2`.*

### 2. Execute — Standard Mode
```bash
# Default (Fuzzy + Multithreaded) with basic dataset:
./genome_firewall sample.fasta markers.csv

# Full real dataset — MDR E. coli vs 53-marker database:
./genome_firewall --input sample_real.fasta --db markers_large.csv --fuzzy --async

# Exact-only + serial (useful for deterministic debugging):
./genome_firewall --input sample.fasta --db markers.csv --exact --serial
```

### 3. Execute — Interactive REPL Shell
```bash
./genome_firewall --interactive
```

Then use these REPL commands (or pipe from `repl_commands.txt`):
```
genome_firewall> status
genome_firewall> load_db markers_large.csv
genome_firewall> scan sample_real.fasta
genome_firewall> set_fuzzy on
genome_firewall> set_conf 70.0
genome_firewall> exit
```

You can also pipe the demo session:
```bash
./genome_firewall --interactive < repl_commands.txt
```

### 4. Sample Output
```
========================================================================
               GENOME FIREWALL v2.0 - BIOSECURITY CLI
       Predictive Antimicrobial Resistance Decision Engine
========================================================================
[*] System Mode: Mutation-Tolerant (Hamming <= 1) | Multithreaded (std::async)
[*] Confidence Threshold: 75%

[*] Loading AMR Reference Catalog from: markers_large.csv...
[*] Routing to Native CSV Parser for: markers_large.csv
[+] Successfully ingested 53 markers into the Trie Arena.
    -> Indexed 53 AMR signatures in 4-ary Prefix Trie (4821 nodes, Arena: 376 KB)
    -> Fast-Rejection Bloom Filter initialized (1048576 bits)
    -> Load & Index Time: 4.231 ms

[*] Initializing Zero-Copy Sequence Ingestion for: sample_real.fasta...
[+] Memory-Mapped Zero-Copy I/O active for sample_real.fasta (4817 bytes mapped).

+---------------------------------------------------------------------------------------------------+
|             GENOME FIREWALL v2.0 - LOGISTIC REGRESSION & BIOSECURITY DECISION REPORT              |
+---------------------------------------------------------------------------------------------------+
| ANTIBIOTIC       | PREDICTION      | PROBABILITY   | CONFIDENCE  | EVIDENCE   | CLINICAL RECOMMENDATION  |
+---------------------------------------------------------------------------------------------------+
| Amoxicillin      | LIKELY TO FAIL  | 0.9801        | 98.0%       | [Tier I]   | DO NOT PRESCRIBE (Resistant) |
| Meropenem        | LIKELY TO FAIL  | 0.9677        | 96.8%       | [Tier I]   | DO NOT PRESCRIBE (Resistant) |
| Ciprofloxacin    | LIKELY TO FAIL  | 0.9543        | 95.4%       | [Tier I]   | DO NOT PRESCRIBE (Resistant) |
| Tetracycline     | LIKELY TO FAIL  | 0.9321        | 93.2%       | [Tier I]   | DO NOT PRESCRIBE (Resistant) |
| Vancomycin       | LIKELY TO FAIL  | 0.9212        | 92.1%       | [Tier I]   | DO NOT PRESCRIBE (Resistant) |
| Erythromycin     | LIKELY TO FAIL  | 0.9108        | 91.1%       | [Tier I]   | DO NOT PRESCRIBE (Resistant) |
| Rifampin         | NO-CALL         | 0.5234        | 52.3%       | [Tier II]  | PROCEED WITH CLINICAL CAUTION |
| Gentamicin       | LIKELY TO WORK  | 0.1012        | 89.9%       | [Tier III] | VIABLE DRUG TARGET (Susceptible) |
+---------------------------------------------------------------------------------------------------+
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

## 🧪 Validation
*Developed from scratch in C++14. Validated for 0 memory leaks via Valgrind (Linux). AMR marker sequences sourced from NCBI/CARD/ResFinder public databases.*
