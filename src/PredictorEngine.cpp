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

        // Map P into Status and Clinical Recommendation.
        // A call is only emitted when the scaled confidence clears the user's
        // threshold; otherwise the result is deliberately downgraded to NO-CALL.
        if (pred.probability > 0.85) {
            pred.confidenceScore = pred.probability * 100.0;
            if (pred.confidenceScore >= confidenceThreshold) {
                pred.status = PredictionStatus::LIKELY_TO_FAIL;
                pred.tier = (pred.directHits > 0) ? EvidenceTier::TIER_I : EvidenceTier::TIER_II;
                pred.clinicalRecommendation = "DO NOT PRESCRIBE (Resistant)";
            } else {
                pred.status = PredictionStatus::NO_CALL;
                pred.tier = EvidenceTier::TIER_II;
                pred.clinicalRecommendation = "PROCEED WITH CLINICAL CAUTION";
            }
        } else if (pred.probability < 0.15) {
            pred.confidenceScore = (1.0 - pred.probability) * 100.0;
            if (pred.confidenceScore >= confidenceThreshold) {
                pred.status = PredictionStatus::LIKELY_TO_WORK;
                pred.tier = EvidenceTier::TIER_III;
                pred.clinicalRecommendation = "VIABLE DRUG TARGET (Susceptible)";
            } else {
                pred.status = PredictionStatus::NO_CALL;
                pred.tier = EvidenceTier::TIER_II;
                pred.clinicalRecommendation = "PROCEED WITH CLINICAL CAUTION";
            }
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

namespace {
// Minimal JSON string escaper — input includes user-controlled file names.
std::string jsonEscape(const std::string& value) {
    std::string out;
    out.reserve(value.size() + 8);
    for (unsigned char c : value) {
        switch (c) {
            case '"':  out += "\\\""; break;
            case '\\': out += "\\\\"; break;
            case '\b': out += "\\b";  break;
            case '\f': out += "\\f";  break;
            case '\n': out += "\\n";  break;
            case '\r': out += "\\r";  break;
            case '\t': out += "\\t";  break;
            default:
                if (c < 0x20) {
                    std::ostringstream hex;
                    hex << "\\u" << std::hex << std::setw(4) << std::setfill('0') << (int)c;
                    out += hex.str();
                } else {
                    out += static_cast<char>(c);
                }
        }
    }
    return out;
}
} // namespace

std::string PredictorEngine::generateJSONReport(const std::vector<DrugPrediction>& predictions, const ScanContext& ctx) const {
    std::ostringstream oss;
    oss << std::fixed << std::setprecision(6);

    const auto failCount = std::count_if(predictions.begin(), predictions.end(),
        [](const DrugPrediction& p) { return p.status == PredictionStatus::LIKELY_TO_FAIL; });
    const auto workCount = std::count_if(predictions.begin(), predictions.end(),
        [](const DrugPrediction& p) { return p.status == PredictionStatus::LIKELY_TO_WORK; });
    const auto noCallCount = std::count_if(predictions.begin(), predictions.end(),
        [](const DrugPrediction& p) { return p.status == PredictionStatus::NO_CALL; });

    oss << "{";
    oss << "\"schemaVersion\":2,";

    oss << "\"engine\":{"
        << "\"name\":\"Genome Firewall\","
        << "\"version\":\"2.0\","
        << "\"systemMode\":\"" << jsonEscape(ctx.systemMode) << "\","
        << "\"threading\":\"" << jsonEscape(ctx.threading) << "\","
        << "\"kmerLength\":" << ctx.kmerLength << ","
        << "\"chunkSize\":" << ctx.chunkSize << ","
        << "\"confidenceThreshold\":" << ctx.confidenceThreshold
        << "},";

    oss << "\"input\":{"
        << "\"fileName\":\"" << jsonEscape(ctx.inputFile) << "\","
        << "\"bytes\":" << ctx.totalBytesScanned << ","
        << "\"chunks\":" << ctx.chunkCount
        << "},";

    oss << "\"database\":{"
        << "\"file\":\"" << jsonEscape(ctx.databaseFile) << "\","
        << "\"markers\":" << ctx.markerCount << ","
        << "\"trieNodes\":" << ctx.trieNodes << ","
        << "\"trieMemoryKB\":" << ctx.trieMemoryKB << ","
        << "\"bloomBits\":" << ctx.bloomBits << ","
        << "\"loadTimeMs\":" << ctx.dbTimeMs
        << "},";

    oss << "\"telemetry\":{"
        << "\"dbLoadTimeMs\":" << ctx.dbTimeMs << ","
        << "\"scanTimeMs\":" << ctx.scanTimeMs << ","
        << "\"totalTimeMs\":" << ctx.totalTimeMs << ","
        << "\"totalBytesScanned\":" << ctx.totalBytesScanned << ","
        << "\"chunkCount\":" << ctx.chunkCount << ","
        << "\"trieNodes\":" << ctx.trieNodes << ","
        << "\"memoryKB\":" << ctx.trieMemoryKB << ","
        << "\"throughputMBps\":" << (ctx.scanTimeMs > 0.0
            ? (static_cast<double>(ctx.totalBytesScanned) / (1024.0 * 1024.0)) / (ctx.scanTimeMs / 1000.0)
            : 0.0)
        << "},";

    oss << "\"summary\":{"
        << "\"drugCount\":" << predictions.size() << ","
        << "\"likelyToFail\":" << failCount << ","
        << "\"likelyToWork\":" << workCount << ","
        << "\"noCall\":" << noCallCount << ","
        << "\"exactHits\":" << ctx.exactHits << ","
        << "\"mutatedHits\":" << ctx.mutatedHits
        << "},";

    oss << "\"predictions\":[";
    for (size_t i = 0; i < predictions.size(); ++i) {
        const auto& p = predictions[i];
        oss << "{";
        oss << "\"drugName\":\"" << jsonEscape(p.drugName) << "\",";
        oss << "\"status\":\"" << statusToString(p.status) << "\",";
        oss << "\"probability\":" << p.probability << ",";
        oss << "\"confidenceScore\":" << p.confidenceScore << ",";
        oss << "\"evidenceTier\":\"" << tierToString(p.tier) << "\",";
        oss << "\"clinicalRecommendation\":\"" << jsonEscape(p.clinicalRecommendation) << "\",";
        oss << "\"directHits\":" << p.directHits << ",";
        oss << "\"mutatedHits\":" << p.mutatedHits << ",";
        oss << "\"targetPresent\":" << (p.targetPresent ? "true" : "false");
        oss << "}";
        if (i + 1 < predictions.size()) oss << ",";
    }
    oss << "]";
    oss << "}";
    return oss.str();
}
