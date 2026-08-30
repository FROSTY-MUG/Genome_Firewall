#ifndef LOGISTIC_REGRESSOR_H
#define LOGISTIC_REGRESSOR_H

#include <vector>
#include <string>
#include <unordered_map>
#include <cmath>

// Model weights structure for an individual antibiotic
struct DrugModelWeights {
    std::string drugName;
    std::vector<double> weights; // Weight vector W matching binary feature vector size X
    double bias{0.0};            // Bias term b
};

class LogisticRegressor {
public:
    LogisticRegressor();
    ~LogisticRegressor() = default;

    // Calculate linear combination z = W * X + b
    double computeDotProduct(const std::vector<double>& weights, const std::vector<int>& featureVector, double bias) const;

    // Sigmoid activation function: 1.0 / (1.0 + exp(-z))
    double sigmoid(double z) const;

    // Inference: Calculate resistance probability P for target drug given Binary Feature Vector X
    double predictProbability(const std::string& drugName, const std::vector<int>& featureVector) const;

    // Initialize pre-trained weight matrices for target antibiotics
    void initializePretrainedWeights(size_t featureVectorSize);

    const std::unordered_map<std::string, DrugModelWeights>& getModels() const { return drugModels; }

private:
    std::unordered_map<std::string, DrugModelWeights> drugModels;
};

#endif // LOGISTIC_REGRESSOR_H
