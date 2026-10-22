#include "PredictorEngine.h"
#include <sstream>
#include <iomanip>
#include <cmath>
#include <algorithm>
#include <iostream>

PredictorEngine::PredictorEngine(const AMRTrie& trie, double confidenceThreshold)
    : trie(trie), confidenceThreshold(confidenceThreshold) {}

void PredictorEngine::mergeHits(const std::unordered_map<std::string, int>& direct,
                               const std::unordered_map<std::string, int>& mutated,
                               const std::unordered_map<std::string, bool>& targetIntegrity,
                               int directCount, int mutatedCount) {
    directHitCount += directCount;
    mutatedHitCount += mutatedCount;

    for (const auto& kv : direct) {
        drugDirectMatches[kv.first] += kv.second;
    }
    for (const auto& kv : mutated) {
        drugMutatedMatches[kv.first] += kv.second;
    }
    for (const auto& kv : targetIntegrity) {
        if (kv.second) {
            drugTargetIntegrity[kv.first] = true;
        }
    }
}

void PredictorEngine::analyzeSequenceFeatures(const std::string& sequence, size_t kmerLength, bool enableFuzzy) {
    if (sequence.length() < kmerLength) return;

    for (size_t i = 0; i <= sequence.length() - kmerLength; ++i) {
        std::string kmer = sequence.substr(i, kmerLength);
        int recIdx = -1;

        if (trie.searchExact(kmer, &recIdx)) {
            directHitCount++;
        } else if (enableFuzzy && trie.searchWithMismatch(kmer, 1, &recIdx)) {
            mutatedHitCount++;
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

    // Default drugs if none found in DB
    if (allDrugs.empty()) {
        allDrugs["Amoxicillin"] = true;
        allDrugs["Tetracycline"] = true;
        allDrugs["Erythromycin"] = true;
        allDrugs["Gentamicin"] = true;
        allDrugs["Chloramphenicol"] = true;
    }

    std::vector<DrugPrediction> predictions;

    for (const auto& pair : allDrugs) {
        std::string drug = pair.first;
        DrugPrediction pred;
        pred.drugName = drug;

        auto dIt = drugDirectMatches.find(drug);
        pred.directHits = (dIt != drugDirectMatches.end()) ? dIt->second : 0;

        auto mIt = drugMutatedMatches.find(drug);
        pred.mutatedHits = (mIt != drugMutatedMatches.end()) ? mIt->second : 0;

        auto tIt = drugTargetIntegrity.find(drug);
        pred.targetPresent = (tIt != drugTargetIntegrity.end()) ? tIt->second : false;

        // Model Inference: P(Resistance)
        pred.probability = regressor.predictProbability(drug, featureVector);

        // Map P into Status and Clinical Recommendation
        if (pred.probability > 0.85) {
            pred.status = PredictionStatus::LIKELY_TO_FAIL;
            pred.confidenceScore = pred.probability * 100.0;
            pred.tier = (pred.directHits > 0) ? EvidenceTier::TIER_I : EvidenceTier::TIER_II;
            pred.clinicalRecommendation = "DO NOT PRESCRIBE (Resistant)";
        } else if (pred.probability < 0.15) {
            pred.status = PredictionStatus::LIKELY_TO_WORK;
            pred.confidenceScore = (1.0 - pred.probability) * 100.0;
            pred.tier = EvidenceTier::TIER_III;
            pred.clinicalRecommendation = "VIABLE DRUG TARGET (Susceptible)";
        } else {
            pred.status = PredictionStatus::NO_CALL;
            pred.confidenceScore = (1.0 - std::abs(pred.probability - 0.5) * 2.0) * 100.0;
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
        case PredictionStatus::NO_CALL:        return "NO-CALL";
        default:                               return "UNKNOWN";
    }
}

std::string PredictorEngine::tierToString(EvidenceTier tier) const {
    switch (tier) {
        case EvidenceTier::TIER_I:   return "[Tier I]";
        case EvidenceTier::TIER_II:  return "[Tier II]";
        case EvidenceTier::TIER_III: return "[Tier III]";
        default:                     return "[Unclassified]";
    }
}

std::string PredictorEngine::generateASCIIReport(const std::vector<DrugPrediction>& predictions, double elapsedMs, size_t totalBytesScanned) const {
    std::stringstream ss;
    std::string hLine = "+---------------------------------------------------------------------------------------------------+";

    ss << "\n" << hLine << "\n";
    ss << "|             GENOME FIREWALL v2.0 - LOGISTIC REGRESSION & BIOSECURITY DECISION REPORT              |\n";
    ss << hLine << "\n";
    ss << "| " << std::left << std::setw(16) << "ANTIBIOTIC"
       << "| " << std::setw(16) << "PREDICTION"
       << "| " << std::setw(14) << "PROBABILITY"
       << "| " << std::setw(12) << "CONFIDENCE"
       << "| " << std::setw(11) << "EVIDENCE"
       << "| " << std::setw(25) << "CLINICAL RECOMMENDATION" << " |\n";
    ss << hLine << "\n";

    for (const auto& pred : predictions) {
        std::stringstream probStr, confStr;
        probStr << std::fixed << std::setprecision(4) << pred.probability;
        confStr << std::fixed << std::setprecision(1) << pred.confidenceScore << "%";

        ss << "| " << std::left << std::setw(16) << pred.drugName
           << "| " << std::setw(16) << statusToString(pred.status)
           << "| " << std::setw(14) << probStr.str()
           << "| " << std::setw(12) << confStr.str()
           << "| " << std::setw(11) << tierToString(pred.tier)
           << "| " << std::setw(25) << pred.clinicalRecommendation << " |\n";
    }

    ss << hLine << "\n";
    double mbScanned = static_cast<double>(totalBytesScanned) / (1024.0 * 1024.0);
    double mbPerSec = (elapsedMs > 0.0) ? (mbScanned / (elapsedMs / 1000.0)) : 0.0;

    ss << "| Telemetry: " << (totalBytesScanned / 1024) << " KB Scanned | Inference Time: "
       << std::fixed << std::setprecision(2) << elapsedMs << " ms ("
       << std::setprecision(2) << mbPerSec << " MB/s) |\n";
    ss << hLine << "\n";
    ss << "WARNING: Defensive decision support only. All results must be confirmed by standard\n"
       << "laboratory antimicrobial susceptibility testing before clinical application.\n";
    ss << hLine << "\n\n";

    return ss.str();
}
