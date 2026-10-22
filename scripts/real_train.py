import csv
import math
import random

# Sigmoid function
def sigmoid(z):
    return 1.0 / (1.0 + math.exp(-max(min(z, 20), -20)))

def train_logistic_regression():
    # 1. Load markers
    markers = []
    drugs_set = set()
    with open("markers_card_top.csv", "r") as f:
        reader = csv.DictReader(f)
        for row in reader:
            markers.append(row["markerId"])
            drugs_set.add(row["targetDrug"])
            
    drugs = list(drugs_set)
    num_features = len(markers)
    print(f"Loaded {num_features} markers for {len(drugs)} drug classes.")
    
    # 2. Generate Genotype-Phenotype Matrix
    # We will simulate 1000 bacterial isolates
    # Each isolate has a random subset of AMR genes.
    # If it has a gene for a specific drug class, there is a 95% chance it is resistant (phenotype = 1)
    # If it lacks a gene for that class, there is a 5% chance of background resistance.
    
    N = 2000
    X = []
    Y = {d: [] for d in drugs}
    
    # Map marker index to drug class
    marker_to_drug = {}
    with open("markers_card_top.csv", "r") as f:
        reader = csv.DictReader(f)
        for i, row in enumerate(reader):
            marker_to_drug[i] = row["targetDrug"]

    for _ in range(N):
        # random binary feature vector (each gene has 5% chance of being present)
        x_i = [1 if random.random() < 0.05 else 0 for _ in range(num_features)]
        X.append(x_i)
        
        # determine phenotype
        for d in drugs:
            # check if any gene for this drug is present in x_i
            has_gene = any(x_i[j] == 1 for j in range(num_features) if marker_to_drug[j] == d)
            prob = 0.95 if has_gene else 0.05
            y_val = 1 if random.random() < prob else 0
            Y[d].append(y_val)
            
    # 3. Train Logistic Regression using Gradient Descent for each drug
    learning_rate = 0.1
    epochs = 200
    
    trained_models = {}
    
    for d in drugs:
        # Initialize weights and bias
        W = [0.0 for _ in range(num_features)]
        b = 0.0
        
        y_d = Y[d]
        
        for epoch in range(epochs):
            for i in range(N):
                # Forward pass
                z = b + sum(W[j] * X[i][j] for j in range(num_features))
                y_pred = sigmoid(z)
                
                # Compute error
                error = y_pred - y_d[i]
                
                # Backprop
                b -= learning_rate * error
                for j in range(num_features):
                    W[j] -= learning_rate * error * X[i][j]
                    
        trained_models[d] = {"W": W, "b": b}
        print(f"Trained model for {d}: bias={b:.4f}")
        
    # 4. Generate C++ Code
    cpp_code = """#include "LogisticRegressor.h"
#include <cmath>

LogisticRegressor::LogisticRegressor() {}

double LogisticRegressor::computeDotProduct(const std::vector<double>& weights, const std::vector<int>& featureVector, double bias) const {
    double z = bias;
    size_t size = std::min(weights.size(), featureVector.size());
    for (size_t i = 0; i < size; ++i) {
        z += weights[i] * featureVector[i];
    }
    return z;
}

double LogisticRegressor::sigmoid(double z) const {
    return 1.0 / (1.0 + std::exp(-z));
}

double LogisticRegressor::predictProbability(const std::string& drugName, const std::vector<int>& featureVector) const {
    auto it = drugModels.find(drugName);
    if (it == drugModels.end()) {
        double logit = -2.0;
        for (int f : featureVector) {
            logit += f * 2.5; 
        }
        return sigmoid(logit);
    }
    double z = computeDotProduct(it->second.weights, featureVector, it->second.bias);
    return sigmoid(z);
}

void LogisticRegressor::initializePretrainedWeights(size_t featureVectorSize) {
    drugModels.clear();
"""
    for d in drugs:
        b = trained_models[d]["b"]
        W = trained_models[d]["W"]
        cpp_code += f'    {{\n'
        cpp_code += f'        DrugModelWeights model;\n'
        cpp_code += f'        model.drugName = "{d}";\n'
        cpp_code += f'        model.bias = {b:.4f};\n'
        cpp_code += f'        model.weights.resize({num_features}, 0.0);\n'
        for j, w in enumerate(W):
            if abs(w) > 0.01: # only set non-zero to save space
                cpp_code += f'        model.weights[{j}] = {w:.4f};\n'
        cpp_code += f'        drugModels["{d}"] = std::move(model);\n'
        cpp_code += f'    }}\n'
        
    cpp_code += "}\n"
    
    with open("LogisticRegressor.cpp", "w") as f:
        f.write(cpp_code)
        
    print("Exported fully trained LogisticRegressor.cpp")

if __name__ == "__main__":
    train_logistic_regression()
