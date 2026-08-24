#pragma once

#include <vector>
#include <cmath>
#include <random>
#include <algorithm>
#include <memory>
#include <unordered_map>
#include <string>
#include <iostream>

#include "../../../compiler/ast/ast.h"
#include "../../../compiler/ast/value.h"

// ============================================================================
// TENSOR — Core Data Structure
// ============================================================================

struct Tensor {
    std::vector<std::vector<double>> data;
    int rows, cols;
    bool requires_grad;
    Tensor* grad;
    
    Tensor() : rows(0), cols(0), requires_grad(false), grad(nullptr) {}
    
    Tensor(int r, int c, bool requires_grad = false) 
        : rows(r), cols(c), requires_grad(requires_grad), grad(nullptr) {
        data.resize(r, std::vector<double>(c, 0.0));
    }
    
    Tensor(const std::vector<std::vector<double>>& d, bool requires_grad = false)
        : data(d), rows(d.size()), cols(d.empty() ? 0 : d[0].size()), 
          requires_grad(requires_grad), grad(nullptr) {}
    
    ~Tensor() { delete grad; }
    
    double& operator()(int i, int j) { return data[i][j]; }
    const double& operator()(int i, int j) const { return data[i][j]; }
    
    void fill(double val) {
        for (auto& row : data) std::fill(row.begin(), row.end(), val);
    }
    
    void randomize(double min = -1.0, double max = 1.0) {
        static std::random_device rd;
        static std::mt19937 gen(rd());
        std::uniform_real_distribution<double> dist(min, max);
        for (auto& row : data) {
            for (auto& v : row) v = dist(gen);
        }
    }
    
    Tensor* clone() const {
        return new Tensor(data, requires_grad);
    }
    
    std::vector<int> shape() const { return {rows, cols}; }
    int size() const { return rows * cols; }
    
    std::string toString() const {
        std::string s = "Tensor([";
        for (int i = 0; i < rows && i < 5; i++) {
            if (i > 0) s += "         ";
            s += "[";
            for (int j = 0; j < cols && j < 5; j++) {
                s += std::to_string(data[i][j]);
                if (j < cols - 1 && j < 4) s += ", ";
            }
            if (cols > 5) s += ", ...";
            s += "]";
            if (i < rows - 1) s += "\n";
        }
        if (rows > 5) s += "\n         ...";
        s += "])";
        return s;
    }
};

// ============================================================================
// LAYER — Base Class
// ============================================================================

struct Layer {
    std::string name;
    virtual ~Layer() = default;
    
    virtual Tensor* forward(Tensor* input) = 0;
    virtual void backward(Tensor* grad_output) = 0;
    virtual void update(double learning_rate) = 0;
    virtual void zero_grad() = 0;
    virtual std::vector<Tensor*> parameters() = 0;
};

// ============================================================================
// DENSE (Fully Connected) Layer
// ============================================================================

struct DenseLayer : public Layer {
    Tensor* weights;
    Tensor* bias;
    Tensor* input_cache;
    Tensor* grad_weights;
    Tensor* grad_bias;
    std::string activation;
    
    DenseLayer(int input_size, int output_size, std::string act = "relu", std::string name = "dense")
        : activation(act) {
        this->name = name;
        weights = new Tensor(input_size, output_size);
        bias = new Tensor(1, output_size);
        grad_weights = new Tensor(input_size, output_size);
        grad_bias = new Tensor(1, output_size);
        input_cache = nullptr;
        
        double scale = (activation == "relu") ? std::sqrt(2.0 / input_size) : std::sqrt(1.0 / input_size);
        weights->randomize(-scale, scale);
        bias->randomize(-0.01, 0.01);
    }
    
    ~DenseLayer() {
        delete weights;
        delete bias;
        delete grad_weights;
        delete grad_bias;
        delete input_cache;
    }
    
    Tensor* forward(Tensor* input) override {
        delete input_cache;
        input_cache = new Tensor(input->data);
        
        Tensor* output = new Tensor(input->rows, weights->cols);
        
        for (int i = 0; i < input->rows; i++) {
            for (int j = 0; j < weights->cols; j++) {
                double sum = 0.0;
                for (int k = 0; k < input->cols; k++) {
                    sum += input->data[i][k] * weights->data[k][j];
                }
                output->data[i][j] = sum + bias->data[0][j];
            }
        }
        
        if (activation == "relu") {
            for (auto& row : output->data) {
                for (auto& v : row) v = (v > 0) ? v : 0.0;
            }
        } else if (activation == "sigmoid") {
            for (auto& row : output->data) {
                for (auto& v : row) v = 1.0 / (1.0 + std::exp(-v));
            }
        } else if (activation == "tanh") {
            for (auto& row : output->data) {
                for (auto& v : row) v = std::tanh(v);
            }
        }
        
        return output;
    }
    
    void backward(Tensor* grad_output) override {
        for (int i = 0; i < weights->rows; i++) {
            for (int j = 0; j < weights->cols; j++) {
                double sum = 0.0;
                for (int k = 0; k < input_cache->rows; k++) {
                    sum += input_cache->data[k][i] * grad_output->data[k][j];
                }
                grad_weights->data[i][j] = sum;
            }
        }
        
        for (int j = 0; j < bias->cols; j++) {
            double sum = 0.0;
            for (int k = 0; k < grad_output->rows; k++) {
                sum += grad_output->data[k][j];
            }
            grad_bias->data[0][j] = sum;
        }
    }
    
    void update(double learning_rate) override {
        for (int i = 0; i < weights->rows; i++) {
            for (int j = 0; j < weights->cols; j++) {
                weights->data[i][j] -= learning_rate * grad_weights->data[i][j];
            }
        }
        
        for (int j = 0; j < bias->cols; j++) {
            bias->data[0][j] -= learning_rate * grad_bias->data[0][j];
        }
    }
    
    void zero_grad() override {
        grad_weights->fill(0.0);
        grad_bias->fill(0.0);
    }
    
    std::vector<Tensor*> parameters() override {
        return {weights, bias};
    }
};

// ============================================================================
// ACTIVATION LAYER
// ============================================================================

struct ActivationLayer : public Layer {
    std::string activation;
    Tensor* input_cache;
    
    ActivationLayer(std::string act, std::string name = "activation") 
        : activation(act), input_cache(nullptr) {
        this->name = name;
    }
    
    ~ActivationLayer() { delete input_cache; }
    
    Tensor* forward(Tensor* input) override {
        delete input_cache;
        input_cache = new Tensor(input->data);
        
        Tensor* output = new Tensor(input->data);
        
        if (activation == "relu") {
            for (auto& row : output->data) {
                for (auto& v : row) v = (v > 0) ? v : 0.0;
            }
        } else if (activation == "sigmoid") {
            for (auto& row : output->data) {
                for (auto& v : row) v = 1.0 / (1.0 + std::exp(-v));
            }
        } else if (activation == "tanh") {
            for (auto& row : output->data) {
                for (auto& v : row) v = std::tanh(v);
            }
        } else if (activation == "softmax") {
            for (int i = 0; i < output->rows; i++) {
                double max_val = *std::max_element(output->data[i].begin(), output->data[i].end());
                double sum = 0.0;
                for (auto& v : output->data[i]) {
                    v = std::exp(v - max_val);
                    sum += v;
                }
                for (auto& v : output->data[i]) v /= sum;
            }
        }
        
        return output;
    }
    
    void backward(Tensor* grad_output) override {}
    void update(double learning_rate) override {}
    void zero_grad() override {}
    std::vector<Tensor*> parameters() override { return {}; }
};

// ============================================================================
// DROPOUT LAYER
// ============================================================================

struct DropoutLayer : public Layer {
    double dropout_rate;
    bool training;
    std::vector<std::vector<int>> mask;
    
    DropoutLayer(double rate, std::string name = "dropout") 
        : dropout_rate(rate), training(true) {
        this->name = name;
    }
    
    Tensor* forward(Tensor* input) override {
        Tensor* output = new Tensor(input->data);
        mask.resize(input->rows, std::vector<int>(input->cols, 1));
        
        if (training && dropout_rate > 0) {
            static std::random_device rd;
            static std::mt19937 gen(rd());
            std::bernoulli_distribution dist(1.0 - dropout_rate);
            
            for (int i = 0; i < input->rows; i++) {
                for (int j = 0; j < input->cols; j++) {
                    if (!dist(gen)) {
                        output->data[i][j] = 0.0;
                        mask[i][j] = 0;
                    }
                }
            }
        }
        
        return output;
    }
    
    void backward(Tensor* grad_output) override {
        for (int i = 0; i < grad_output->rows; i++) {
            for (int j = 0; j < grad_output->cols; j++) {
                grad_output->data[i][j] *= mask[i][j];
            }
        }
    }
    
    void update(double learning_rate) override {}
    void zero_grad() override {}
    std::vector<Tensor*> parameters() override { return {}; }
    
    void set_training(bool train) { training = train; }
};

// ============================================================================
// LOSS FUNCTIONS
// ============================================================================

namespace Loss {
    inline double mse(Tensor* predicted, Tensor* target) {
        double sum = 0.0;
        for (int i = 0; i < predicted->rows; i++) {
            for (int j = 0; j < predicted->cols; j++) {
                double diff = predicted->data[i][j] - target->data[i][j];
                sum += diff * diff;
            }
        }
        return sum / (predicted->rows * predicted->cols);
    }
    
    inline double cross_entropy(Tensor* predicted, Tensor* target) {
        double sum = 0.0;
        for (int i = 0; i < predicted->rows; i++) {
            for (int j = 0; j < predicted->cols; j++) {
                double p = std::max(predicted->data[i][j], 1e-15);
                sum += target->data[i][j] * std::log(p);
            }
        }
        return -sum / predicted->rows;
    }
}

// ============================================================================
// OPTIMIZERS
// ============================================================================

struct Optimizer {
    double learning_rate;
    virtual ~Optimizer() = default;
    virtual void step(std::vector<Layer*>& layers) = 0;
};

struct SGDOptimizer : public Optimizer {
    SGDOptimizer(double lr) { learning_rate = lr; }
    
    void step(std::vector<Layer*>& layers) override {
        for (auto* layer : layers) {
            if (layer) layer->update(learning_rate);
        }
    }
};

struct AdamOptimizer : public Optimizer {
    double beta1, beta2, epsilon;
    double t;
    std::unordered_map<Tensor*, Tensor*> m, v;
    
    AdamOptimizer(double lr = 0.001, double b1 = 0.9, double b2 = 0.999, double eps = 1e-8)
        : beta1(b1), beta2(b2), epsilon(eps), t(0) {
        learning_rate = lr;
    }
    
    void step(std::vector<Layer*>& layers) override {
        t += 1.0;
        // Placeholder for Adam update
        for (auto* layer : layers) {
            for (auto* param : layer->parameters()) {
                if (m.find(param) == m.end()) {
                    m[param] = new Tensor(param->rows, param->cols);
                    v[param] = new Tensor(param->rows, param->cols);
                }
            }
        }
    }
};

// ============================================================================
// SEQUENTIAL MODEL
// ============================================================================

struct Sequential {
    std::vector<Layer*> layers;
    std::string name;
    
    Sequential(std::string n = "model") : name(n) {}
    
    ~Sequential() {
        for (auto* layer : layers) delete layer;
    }
    
    void add(Layer* layer) {
        layers.push_back(layer);
    }
    
    Tensor* forward(Tensor* input) {
        Tensor* current = input;
        for (auto* layer : layers) {
            current = layer->forward(current);
        }
        return current;
    }
    
    void train(Tensor* input, Tensor* target, Optimizer& optimizer, int epochs = 100, bool verbose = true) {
        for (int epoch = 0; epoch < epochs; epoch++) {
            Tensor* output = forward(input);
            double loss = Loss::mse(output, target);
            optimizer.step(layers);
            
            if (verbose && (epoch % 10 == 0 || epoch == epochs - 1)) {
                printf("Epoch %d/%d - Loss: %.6f\n", epoch+1, epochs, loss);
            }
            
            for (auto* layer : layers) {
                layer->zero_grad();
            }
            
            delete output;
        }
    }
    
    Tensor* predict(Tensor* input) {
        return forward(input);
    }
};

// ============================================================================
// NATIVE FUNCTION DECLARATIONS
// ============================================================================

namespace VMLNative {
    // Tensor creation
    Value native_tensor(std::vector<Value>& args);
    Value native_tensor_zeros(std::vector<Value>& args);
    Value native_tensor_ones(std::vector<Value>& args);
    Value native_tensor_random(std::vector<Value>& args);
    Value native_tensor_eye(std::vector<Value>& args);
    
    // Tensor operations
    Value native_tensor_add(std::vector<Value>& args);
    Value native_tensor_sub(std::vector<Value>& args);
    Value native_tensor_mul(std::vector<Value>& args);
    Value native_tensor_div(std::vector<Value>& args);
    Value native_tensor_matmul(std::vector<Value>& args);
    Value native_tensor_transpose(std::vector<Value>& args);
    Value native_tensor_reshape(std::vector<Value>& args);
    Value native_tensor_sum(std::vector<Value>& args);
    Value native_tensor_mean(std::vector<Value>& args);
    Value native_tensor_max(std::vector<Value>& args);
    Value native_tensor_min(std::vector<Value>& args);
    
    // Layers
    Value native_dense(std::vector<Value>& args);
    Value native_activation(std::vector<Value>& args);
    Value native_dropout(std::vector<Value>& args);
    Value native_flatten(std::vector<Value>& args);
    Value native_reshape_layer(std::vector<Value>& args);
    
    // Model
    Value native_sequential(std::vector<Value>& args);
    Value native_model_add(std::vector<Value>& args);
    Value native_model_train(std::vector<Value>& args);
    Value native_model_predict(std::vector<Value>& args);
    Value native_model_evaluate(std::vector<Value>& args);
    Value native_model_summary(std::vector<Value>& args);
    
    // Loss
    Value native_mse(std::vector<Value>& args);
    Value native_cross_entropy(std::vector<Value>& args);
    Value native_mae(std::vector<Value>& args);
    Value native_binary_cross_entropy(std::vector<Value>& args);
    
    // Optimizers
    Value native_sgd(std::vector<Value>& args);
    Value native_adam(std::vector<Value>& args);
    Value native_rmsprop(std::vector<Value>& args);
}