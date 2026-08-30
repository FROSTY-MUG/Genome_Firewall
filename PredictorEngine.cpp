#include "PredictorEngine.h"
#include <sstream>
#include <iomanip>
#include <cmath>
#include <algorithm>
#include <iostream>

PredictorEngine::PredictorEngine(const AMRTrie& trie, double confidenceThreshold)
    : trie(trie), confidenceThreshold(confidenceThreshold) {}

void PredictorEngine::analyzeSequenceFeatures(const std::string& sequence, size_t kmerLength, bool enableFuzzy) {
    if (sequence.length() < kmerLength) return;

    for (size_t i = 0; i <= sequence.length() - kmerLength; ++i) {
        std::string kmer = sequence.substr(i, kmerLength);
        std::string meta;

        if (trie.searchExact(kmer, &meta)) {
            directHitCount++;
            if (!meta.empty()) {
                size_t p1 = meta.find('|');
                size_t p2 = meta.find('|', p1 + 1);
                size_t p3 = meta.find('|', p2 + 1);

                if (p2 != std::string::npos && p3 != std::string::npos) {
                    std::string drug = meta.substr(p2 + 1, p3 - (p2 + 1));
                    std::string recType = meta.substr(p3 + 1);

                    if (recType == "TargetGene") {
                        drugTargetIntegrity[drug] = true;
                    } else {
                        drugDirectMatches[drug]++;
                    }
                }
            }
        } else if (enableFuzzy && trie.searchWithMismatch(kmer, 1, &meta)) {
            mutatedHitCount++;
            if (!meta.empty()) {
                size_t p1 = meta.find('|');
                size_t p2 = meta.find('|', p1 + 1);
                size_t p3 = meta.find('|', p2 + 1);

                if (p2 != std::string::npos && p3 != std::string::npos) {
                    std::string drug = meta.substr(p2 + 1, p3 - (p2 + 1));
                    std::string recType = meta.substr(p3 + 1);

                    if (recType != "TargetGene") {
                        drugMutatedMatches[drug]++;
                    }
                }
            }
        }
    }
}

std::vector<DrugPrediction> PredictorEngine::evaluatePredictionsWithModel(const AMRDatabase& db, const std::vector<int>& featureVector) const {
    regressor.initializePretrainedWeights(featureVector.size());

    std::unordered_map<std::string, bool> allDrugs;
    for (const auto& rec : db.getRecords()) {
        if (!rec.targetDrug.empty()) {
            allDrugs[rec.targetDrug] = true;
        }
    }

    std::vector<DrugPrediction> predictions;

    for (const auto& kv : allDrugs) {
        std::string drug = kv.first;
        DrugPrediction pred;
        pred.drugName = drug;

        int exactHits = 0;
        auto itExact = drugDirectMatches.find(drug);
        if (itExact != drugDirectMatches.end()) exactHits = itExact->second;

        int mutHits = 0;
        auto itMut = drugMutatedMatches.find(drug);
        if (itMut != drugMutatedMatches.end()) mutHits = itMut->second;

        bool targetIntact = true;
        auto itTarget = drugTargetIntegrity.find(drug);
        if (itTarget != drugTargetIntegrity.end()) targetIntact = itTarget->second;

        pred.directHits = exactHits;
        pred.mutatedHits = mutHits;
        pred.targetPresent = targetIntact;

        // Step 1: Compute Logistic Regression Probability P = sigmoid(W * X + b)
        double p = regressor.predictProbability(drug, featureVector);
        
        // Adjust probability based on exact hits if present
        if (exactHits > 0 && p < 0.85) {
            p = std::max(p, 0.88);
        }

        pred.probability = p;

        // Step 2: Target-Gated Decision Logic & Thresholds
        if (!targetIntact) {
            // Target disruption gate: Override decision
            pred.status = PredictionStatus::LIKELY_TO_FAIL;
            pred.confidenceScore = 96.0;
            pred.tier = EvidenceTier::TIER_I;
            pred.clinicalRecommendation = "CONTRAINDICATED (Target Disrupted)";
        } else if (p > 0.85) {
            // P > 0.85: LIKELY TO FAIL (High confidence of resistance)
            pred.status = PredictionStatus::LIKELY_TO_FAIL;
            pred.confidenceScore = std::min(99.9, p * 100.0);
            pred.tier = (exactHits > 0) ? EvidenceTier::TIER_I : EvidenceTier::TIER_II;
            pred.clinicalRecommendation = "DO NOT PRESCRIBE (Resistant)";
        } else if (p < 0.15 && targetIntact) {
            // P < 0.15 AND molecular target present: LIKELY TO WORK
            pred.status = PredictionStatus::LIKELY_TO_WORK;
            pred.confidenceScore = (1.0 - p) * 100.0;
            pred.tier = EvidenceTier::TIER_III;
            pred.clinicalRecommendation = "VIABLE DRUG TARGET (Susceptible)";
        } else {
            // 0.15 <= P <= 0.85: NO-CALL (Uncertain evidence)
            pred.status = PredictionStatus::NO_CALL;
            pred.confidenceScore = 50.0 + (std::abs(0.5 - p) * 50.0);
            pred.tier = EvidenceTier::TIER_II;
            pred.clinicalRecommendation = "PROCEED WITH CLINICAL CAUTION";
        }

        predictions.push_back(pred);
    }

    return predictions;
}

std::string PredictorEngine::statusToString(PredictionStatus status) const {
    switch (status) {
        case PredictionStatus::LIKELY_TO_FAIL: return "LIKELY TO FAIL";
        case PredictionStatus::LIKELY_TO_WORK: return "LIKELY TO WORK";
        case PredictionStatus::NO_CALL:       return "NO-CALL";
    }
    return "UNKNOWN";
}

std::string PredictorEngine::tierToString(EvidenceTier tier) const {
    switch (tier) {
        case EvidenceTier::TIER_I:   return "[Tier I]";
        case EvidenceTier::TIER_II:  return "[Tier II]";
        case EvidenceTier::TIER_III: return "[Tier III]";
    }
    return "[Tier UNK]";
}

std::string PredictorEngine::generateASCIIReport(const std::vector<DrugPrediction>& predictions, double elapsedMs, size_t totalBytesScanned) const {
    std::ostringstream report;
    std::string sep  = "+---------------------------------------------------------------------------------------------------+\n";
    std::string dash = "+---------------------------------------------------------------------------------------------------+\n";

    double throughputMBs = 0.0;
    if (elapsedMs > 0.0) {
        throughputMBs = (static_cast<double>(totalBytesScanned) / (1024.0 * 1024.0)) / (elapsedMs / 1000.0);
    }

    report << "\n" << sep;
    report << "|             GENOME FIREWALL v2.0 - LOGISTIC REGRESSION & BIOSECURITY DECISION REPORT              |\n";
    report << sep;
    report << "| " << std::left << std::setw(16) << "ANTIBIOTIC" 
           << " | " << std::left << std::setw(15) << "PREDICTION"
           << " | " << std::left << std::setw(13) << "PROBABILITY"
           << " | " << std::left << std::setw(11) << "CONFIDENCE"
           << " | " << std::left << std::setw(10) << "EVIDENCE"
           << " | " << std::left << std::setw(24) << "CLINICAL RECOMMENDATION" << " |\n";
    report << dash;

    for (const auto& p : predictions) {
        std::ostringstream probStr, confStr;
        probStr << std::fixed << std::setprecision(4) << p.probability;
        confStr << std::fixed << std::setprecision(1) << p.confidenceScore << "%";

        report << "| " << std::left << std::setw(16) << p.drugName
               << " | " << std::left << std::setw(15) << statusToString(p.status)
               << " | " << std::left << std::setw(13) << probStr.str()
               << " | " << std::left << std::setw(11) << confStr.str()
               << " | " << std::left << std::setw(10) << tierToString(p.tier)
               << " | " << std::left << std::setw(24) << p.clinicalRecommendation << " |\n";
    }

    report << dash;
    report << "| Telemetry: " << (totalBytesScanned / 1024) << " KB Scanned | Inference Time: "
           << std::fixed << std::setprecision(2) << elapsedMs << " ms ("
           << throughputMBs << " MB/s) |\n";
    report << sep;
    report << "WARNING: Defensive decision support only. All results must be confirmed by standard\n"
           << "laboratory antimicrobial susceptibility testing before clinical application.\n";
    report << sep << "\n";

    return report.str();
}
