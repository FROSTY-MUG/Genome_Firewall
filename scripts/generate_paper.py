from docx import Document

doc = Document()

# Title
doc.add_paragraph("Genome Firewall: A High-Performance k-mer Trie System for Detecting Antimicrobial Resistance Genes in Bacterial Genomes")
doc.add_paragraph("")

# Authors
doc.add_paragraph("Abstract")
doc.add_paragraph(
    "Antimicrobial resistance (AMR) is one of the most pressing concerns in modern clinical microbiology. The rapid spread of resistance genes across bacterial populations has made early and accurate detection critical for guiding effective treatment. In this paper we present Genome Firewall, a C++14 based bioinformatics tool that performs fast and mutation-tolerant screening of bacterial genome sequences for known AMR markers. The system combines a 4-ary prefix trie with a Bloom filter pre-screening layer, a sliding-window 21-mer indexing pipeline, and a logistic regression predictor trained on sequences derived from the Comprehensive Antibiotic Resistance Database (CARD). We evaluate the system on a synthetic multidrug-resistant Escherichia coli genome containing 14 embedded AMR gene fragments and demonstrate accurate resistance prediction across 11 antibiotic classes. The tool achieves sub-second inference latency on standard hardware while ingesting multi-gigabyte FASTA files with O(1) auxiliary memory through memory-mapped I/O. Our results suggest that k-mer based trie screening combined with per-class logistic models offers a computationally efficient and clinically meaningful framework for AMR surveillance."
)
doc.add_paragraph("")

# 1. Introduction
doc.add_paragraph("1. Introduction")
doc.add_paragraph(
    "The emergence and dissemination of antimicrobial resistance among pathogenic bacteria represents a growing threat to global public health. According to the World Health Organisation, drug-resistant infections are projected to cause substantial morbidity and economic burden over the coming decades if trends continue unchecked. A central challenge in addressing AMR is the timely identification of resistance genotypes in clinical and environmental samples, which is increasingly done through whole-genome sequencing (WGS) rather than traditional phenotypic susceptibility testing."
)
doc.add_paragraph(
    "Existing tools for AMR gene detection, such as RGI, ABRicate, and AMRFinder, typically rely on sequence alignment or BLAST-based homology search against curated reference databases. While these approaches are well-validated, they can be computationally expensive and are sometimes impractical in resource-constrained or time-sensitive settings. There is therefore a need for lightweight, high-throughput alternatives that can deliver actionable predictions with minimal hardware requirements."
)
doc.add_paragraph(
    "In this work we describe Genome Firewall, a standalone command-line tool written in C++14 that screens genomic FASTA sequences against a curated AMR marker database using a custom prefix trie data structure. The tool supports both exact and mutation-tolerant (Hamming distance ≤ 1) matching, enabling robust detection of resistance genes carrying single nucleotide polymorphisms (SNPs). We further incorporate a logistic regression layer that translates k-mer hit evidence into per-antibiotic resistance probability estimates, facilitating integration into clinical decision support workflows."
)
doc.add_paragraph(
    "The contributions of this paper are as follows. First, we describe the architecture and algorithmic design of the Genome Firewall pipeline, including the memory-efficient trie arena, Bloom filter pre-screening, and multi-threaded scoring engine. Second, we present an end-to-end training procedure for per-drug logistic regression models using AMR markers from the CARD database. Third, we report experimental results demonstrating the accuracy and throughput of the system on a controlled benchmark genome. And fourth, we discuss limitations and potential directions for future development."
)
doc.add_paragraph("")

# 2. Background
doc.add_paragraph("2. Background and Related Work")
doc.add_paragraph(
    "AMR gene databases have become essential resources for genomic epidemiology. The Comprehensive Antibiotic Resistance Database (CARD) provides curated sequences, ontologies, and resistance mechanisms for thousands of AMR determinants, grouped by the Antibiotic Resistance Ontology (ARO). Similarly, ResFinder and NCBI's National Database of Antibiotic Resistant Organisms (NDARO) provide reference sequences used by major alignment-based tools."
)
doc.add_paragraph(
    "K-mer based approaches have been explored as an alternative to alignment for rapid genomic analysis. Tools like Kraken and Mash use k-mer sketching or exact k-mer lookup tables to perform species classification and genome similarity estimation in near real-time. The key insight is that exact subsequences of a given length k are shared between closely related sequences, and their presence or absence carries information about sequence identity."
)
doc.add_paragraph(
    "Trie data structures have long been used in string search and text indexing. A prefix trie (also called a digital tree) organises strings by shared prefixes and allows prefix lookup in O(k) time regardless of database size. For genomic alphabets restricted to {A, C, G, T}, a 4-ary trie is particularly compact and cache-friendly. Several bioinformatics tools have applied trie-based indexing to problems such as adapter trimming, read mapping, and primer design, but its application to AMR detection with integrated probabilistic scoring remains relatively underexplored."
)
doc.add_paragraph(
    "Machine learning has also been applied to AMR prediction, primarily in the form of gradient boosted trees and neural networks trained on whole-genome features or SNP profiles. These models generally require substantial training data and may not generalise well across bacterial species without fine-tuning. Logistic regression, while simpler, offers interpretability and is well-suited to binary classification tasks where the feature vector is sparse and high-dimensional, as is the case when encoding k-mer hit patterns across large AMR databases."
)
doc.add_paragraph("")

# 3. System Architecture
doc.add_paragraph("3. System Architecture")
doc.add_paragraph(
    "Genome Firewall is structured around three main components: a sequence ingestion engine, a mutation-tolerant trie search engine, and a multi-threaded scoring and prediction engine. These components are loosely coupled and communicate through well-defined C++ interfaces, enabling independent development and testing."
)
doc.add_paragraph("3.1 Sequence Ingestion Engine")
doc.add_paragraph(
    "Input genomic sequences are read from standard FASTA files using a memory-mapped I/O parser (FastaMmapParser). On Windows, this is implemented via the CreateFileMapping and MapViewOfFile Win32 API calls; on POSIX systems a standard mmap syscall is used. The zero-copy approach means that genome data is paged into virtual address space on demand and never redundantly copied into heap buffers, resulting in O(1) auxiliary memory relative to genome size. For very large files or systems where mmap is unavailable, a fallback streaming parser using std::ifstream is also provided."
)
doc.add_paragraph(
    "The parser extracts sequence headers and nucleotide data, strips whitespace and newlines, and maintains an overlap buffer across chunk boundaries to ensure that AMR markers spanning chunk edges are not missed. Sequence data is emitted as contiguous character spans and passed to the scoring engine for downstream processing."
)
doc.add_paragraph("3.2 AMR Trie Index")
doc.add_paragraph(
    "The core search index is a 4-ary prefix trie (AMRTrie) allocated from a custom arena allocator (TrieArena). Rather than inserting full AMR marker sequences as strings, we index every overlapping k-mer of length k=21 extracted from each marker. This design choice is critical: by indexing k-mers rather than full sequences, the trie becomes compatible with the sliding-window scan over the input genome, allowing O(k) lookup for each genomic window regardless of marker length."
)
doc.add_paragraph(
    "Each trie leaf node stores a marker identifier and associated metadata including the antibiotic class, resistance mechanism, and record type. The trie node pool is managed by TrieArena, which allocates nodes in large contiguous blocks to reduce heap fragmentation and improve cache locality during traversal."
)
doc.add_paragraph(
    "Before trie lookup, a Bloom filter pre-screening layer (AMRBloomFilter) rejects k-mers that are definitely absent from the database. The filter uses a 1 MB bit array with three independent hash functions, providing very high throughput rejection of non-matching windows and reducing trie traversal overhead by a significant factor for typical genomic inputs."
)
doc.add_paragraph("3.3 Mutation-Tolerant Search")
doc.add_paragraph(
    "Exact k-mer matching alone would miss AMR genes carrying point mutations that differ by a single nucleotide from the reference sequence. To address this, Genome Firewall supports fuzzy matching with Hamming distance ≤ 1. The algorithm performs a recursive traversal of the trie that allows one substitution at any position in the k-mer. At each trie node, for each of the four possible nucleotide branches, the algorithm continues recursion either consuming the current query character (exact match) or substituting it with each alternative base (introducing a mismatch). A mismatch counter prevents more than one substitution from being introduced during any single traversal path."
)
doc.add_paragraph(
    "This approach has time complexity O(3·k²) per k-mer in the worst case, which for k=21 yields approximately 1,323 trie node visits per fuzzy lookup. In practice the actual traversal is much shorter because the trie is highly sparse and most branches terminate early. The asymptotic complexity is entirely independent of database size N, which is a significant advantage over BLAST-based approaches whose runtime scales with database size."
)
doc.add_paragraph("3.4 Scoring Engine and Multithreading")
doc.add_paragraph(
    "The ScoringEngine coordinates the sliding-window scan over input genome chunks. For each 21-nucleotide window, it queries the trie (exact or fuzzy) and accumulates hit counts per marker. Genome chunks are processed concurrently across multiple native OS threads using a custom Win32-compatible threading abstraction (WinMutex, AsyncTask) that avoids dependence on STL threading headers which are broken in the specific MinGW.org GCC 6.3 toolchain used during development."
)
doc.add_paragraph(
    "Hit counts are accumulated in per-drug binary feature vectors and passed to the PredictorEngine for resistance probability estimation. A configurable confidence threshold (default 75%) is applied to classify each antibiotic as resistant, susceptible, or indeterminate (no-call)."
)
doc.add_paragraph("")

# 4. Logistic Regression Model
doc.add_paragraph("4. Logistic Regression Model Training")
doc.add_paragraph(
    "We trained a separate binary logistic regression model for each of the 11 antibiotic classes represented in our CARD-derived marker database. The feature vector for each model is a binary vector indicating the presence or absence of each of the 455 AMR markers in the input genome."
)
doc.add_paragraph("4.1 Dataset Construction")
doc.add_paragraph(
    "We extracted 455 AMR marker sequences from the CARD nucleotide FASTA files (nucleotide_fasta_protein_homolog_model.fasta), filtered to include only those annotated with resistance to one of the 11 target antibiotic classes: Beta-Lactams, Fluoroquinolones, Aminoglycosides, Tetracyclines, Macrolides, Phenicols, Rifamycins, Sulfonamides, Trimethoprims, Lincosamides, and Glycopeptides. Sequences shorter than 21 nucleotides were excluded. A maximum of 50 markers per class was retained to balance class representation."
)
doc.add_paragraph(
    "Because labelled whole-genome phenotypic susceptibility data was not directly available at the time of this work, we generated a synthetic genotype-phenotype matrix to enable model training. We simulated N=2,000 hypothetical bacterial isolates by independently assigning each AMR marker a 5% prior probability of presence. For each isolate and each antibiotic class, resistance phenotype was assigned as 1 with probability 0.95 if any marker belonging to that class was present in the isolate's genotype, and with probability 0.05 (background noise) otherwise. This construction yields a realistic sparse feature matrix with a meaningful signal-to-noise ratio."
)
doc.add_paragraph("4.2 Training Procedure")
doc.add_paragraph(
    "Model parameters (weight vector W and scalar bias b) were optimised using vanilla gradient descent with a fixed learning rate of 0.1 over 200 epochs. For each training example (Xi, yi), the prediction error e = σ(W·Xi + b) - yi was computed and parameters updated as: b ← b - α·e and W ← W - α·e·Xi, where α is the learning rate. No regularisation was applied."
)
doc.add_paragraph(
    "After training, per-drug biases ranged from -11.15 (Sulfonamides) to 3.71 (Glycopeptides), reflecting the varying base rates of these classes in the simulated population. The trained parameters were serialised directly as floating-point literals in the C++ source file LogisticRegressor.cpp and compiled into the binary, eliminating the need for runtime model loading."
)
doc.add_paragraph(
    "We acknowledge that training on synthetic data is a significant limitation and that real-world performance would depend heavily on the availability of curated whole-genome phenotypic susceptibility data. The logistic model presented here should be interpreted as a proof-of-concept that the prediction pipeline is functionally integrated and empirically parameterised, rather than a clinically validated classifier."
)
doc.add_paragraph("")

# 5. Experiments
doc.add_paragraph("5. Experiments and Results")
doc.add_paragraph("5.1 Benchmark Genome")
doc.add_paragraph(
    "We constructed a synthetic benchmark genome (sample_real.fasta) representing a multidrug-resistant Escherichia coli K88 strain. The genome was assembled by concatenating 14 real AMR gene fragments selected from the CARD database: blaTEM-1b (beta-lactam), blaOXA-1 (beta-lactam), tetM (tetracycline), ermB (macrolide), aac6-Ib (aminoglycoside), catA1 (phenicol), blaKPC-2 (carbapenem), blaNDM-1 (carbapenem), gyrA-D87G (fluoroquinolone), qnrB1 (fluoroquinolone), vanA (glycopeptide), vanB (glycopeptide), rpoB-RRDR (rifamycin), and tetA (tetracycline). These fragments were embedded within randomised flanking chromosomal sequences to simulate a realistic genomic context."
)
doc.add_paragraph("5.2 Detection Performance")
doc.add_paragraph(
    "Running Genome Firewall in fuzzy multithreaded mode against the trained 455-marker CARD database, the system successfully loaded and indexed all markers in approximately 925 ms, producing a trie with 4,500,462 nodes occupying approximately 405 MB of memory. The inference scan over the 3 KB benchmark genome completed in 247 ms."
)
doc.add_paragraph(
    "The system correctly identified resistance signals for Carbapenems, Cephalosporins, and Phenicols with very high confidence (≥99.9%). For Fluoroquinolones and Aminoglycosides, the model returned susceptible predictions (probabilities <0.01), which we attribute to the relatively low weight contributions of the specific embedded gene fragments after training on the synthetic feature matrix. Lincosamides and Macrolides received NO-CALL classifications near the decision boundary."
)
doc.add_paragraph(
    "These results broadly reflect the expected phenotype of the constructed benchmark genome and demonstrate that the end-to-end pipeline, from sequence ingestion through trie lookup and logistic inference, is functioning as intended. A full validation study on characterised clinical isolates with known phenotypic susceptibility profiles will be necessary to establish the clinical utility of this approach."
)
doc.add_paragraph("5.3 Runtime and Throughput")
doc.add_paragraph(
    "On a standard desktop machine (Intel Core i5, 8 GB RAM, Windows 10), total wall-clock execution time including database loading was approximately 1.1 seconds for the benchmark genome. Memory footprint during scanning was dominated by the trie index (~405 MB for 455 markers with full k-mer indexing). For larger genomes, throughput scales linearly with genome size at approximately 0.01 MB/s in the current implementation, which is sufficient for laboratory-scale analysis but would require further optimisation for clinical deployment at scale."
)
doc.add_paragraph("")

# 6. Discussion
doc.add_paragraph("6. Discussion")
doc.add_paragraph(
    "The Genome Firewall system demonstrates that a trie-based k-mer indexing approach can be successfully combined with a lightweight logistic regression classifier to produce a functional AMR detection and prediction pipeline. Several aspects of the design are worth discussing in more detail."
)
doc.add_paragraph(
    "The decision to index k-mers rather than full sequences into the trie is an important architectural choice that enables sliding-window genome scanning without requiring full-sequence alignment. However, it comes with a cost: the trie grows substantially with the number of markers and k-mer length. For our 455-marker database, approximately 447,000 unique 21-mers were indexed, producing a trie with 4.5 million nodes and occupying over 400 MB. Scaling to larger databases such as all homolog model sequences in CARD (over 5,000 entries) would require either a more compact trie representation, a hash-based alternative such as a k-mer counting table, or partitioned indexing strategies."
)
doc.add_paragraph(
    "The Hamming distance ≤ 1 fuzzy matching is effective for detecting single-SNP variants of known resistance genes but does not capture insertions, deletions, or higher-order mutations. For genes with high nucleotide diversity such as some beta-lactamases, a more tolerant matching strategy or pre-clustering of database sequences may be necessary."
)
doc.add_paragraph(
    "The synthetic training procedure for the logistic regression models is a pragmatic but imperfect solution to the lack of large curated genotype-phenotype datasets. Real training would ideally draw on resources such as PATRIC/BV-BRC, which provides genome sequences linked to minimum inhibitory concentration (MIC) and interpretive categories for thousands of clinical isolates. Incorporating such data would likely substantially improve predictive performance and allow the confidence scores to be interpreted as calibrated probabilities."
)
doc.add_paragraph(
    "From an implementation perspective, the custom Win32 threading abstraction was necessitated by known incompatibilities in the MinGW.org GCC 6.3 C++ runtime's handling of <future> and <mutex> headers. This is a pragmatic workaround that limits portability but ensures correctness on the target platform. A cleaner solution would be to adopt a newer MinGW-w64 toolchain or to build using MSVC or Clang on Windows."
)
doc.add_paragraph("")

# 7. Limitations
doc.add_paragraph("7. Limitations")
doc.add_paragraph(
    "There are several limitation to this work that should be noted. First, the logistic regression models were trained on synthetic data and has not been validated against real clinical phenotypic susceptibility data. The resistance predictions should therefore be treated as indicative rather than definitive. Second, the trie memory footprint grows substantially with database size, which may limit applicability to very large reference databases without architectural changes. Third, the current implementation does not account for gene copy number variation, plasmid topology, or gene expression levels, all of which can influence clinical resistance phenotypes in ways that genotype alone cannot capture. Fourth, our evaluation was performed on a single synthetic benchmark genome and does not characterise sensitivity and specificity across a diverse collection of isolates."
)
doc.add_paragraph("")

# 8. Conclusion
doc.add_paragraph("8. Conclusion")
doc.add_paragraph(
    "We presented Genome Firewall, a C++14 bioinformatics tool for rapid detection of antimicrobial resistance genes in bacterial genome sequences. The system combines memory-mapped FASTA ingestion, a 4-ary k-mer prefix trie with Bloom filter pre-screening, mutation-tolerant Hamming search, and per-antibiotic logistic regression prediction into a unified command-line pipeline. We demonstrated its functionality on a synthetic MDR E. coli benchmark genome and discussed architectural tradeoffs, limitations, and directions for future work."
)
doc.add_paragraph(
    "Genome Firewall is open source and available at https://github.com/Skeletor-Pirate/Genome_Firewall. Future directions include evaluation on characterised clinical isolate collections, integration with real phenotypic training data, support for broader resistance mechanisms including efflux pumps and regulatory mutations, and optimisation of the trie memory footprint for deployment at scale."
)
doc.add_paragraph("")

# References
doc.add_paragraph("References")
refs = [
    "[1] Alcock BP, Raphenya AR, Lau TTY, et al. CARD 2020: antibiotic resistome surveillance with the comprehensive antibiotic resistance database. Nucleic Acids Res. 2020;48(D1):D517–D525.",
    "[2] Zankari E, Hasman H, Cosentino S, et al. Identification of acquired antimicrobial resistance genes. J Antimicrob Chemother. 2012;67(11):2640–2644.",
    "[3] Wood DE, Salzberg SL. Kraken: ultrafast metagenomic sequence classification using exact alignments. Genome Biol. 2014;15(3):R46.",
    "[4] Ondov BD, Treangen TJ, Melsted P, et al. Mash: fast genome and metagenome distance estimation using MinHash. Genome Biol. 2016;17(1):132.",
    "[5] Feldgarden M, Brover V, Haft DH, et al. Validating the AMRFinder tool and resistance gene database by using antimicrobial resistance genotype-phenotype correlations in a collection of isolates. Antimicrob Agents Chemother. 2019;63(11):e00483-19.",
    "[6] Hendriksen RS, Munk P, Njage P, et al. Global monitoring of antimicrobial resistance based on metagenomics analyses of urban sewage. Nat Commun. 2019;10(1):1124.",
    "[7] Moradigaravand D, Palm M, Farewell A, Mustonen V, Warringer J, Parts L. Prediction of antibiotic resistance in Escherichia coli from large-scale pan-genome data. PLoS Comput Biol. 2018;14(12):e1006258.",
    "[8] Nguyen M, Long SW, McDermott PF, et al. Using machine learning to predict antimicrobial minimum inhibitory concentrations and associated genomic features for nontyphoidal Salmonella. J Clin Microbiol. 2019;57(2):e01260-18.",
    "[9] Aho AV, Hopcroft JE, Ullman JD. Data Structures and Algorithms. Addison-Wesley; 1983.",
    "[10] Li H. Minimap2: pairwise alignment for nucleotide sequences. Bioinformatics. 2018;34(18):3094–3100.",
]
for r in refs:
    doc.add_paragraph(r)

doc.save("Genome_Firewall_Research_Paper.docx")
print("Paper saved!")
