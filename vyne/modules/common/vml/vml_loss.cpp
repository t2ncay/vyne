#include "vml_common.h"

namespace VMLNative {

Value native_mse(std::vector<Value>& args) {
    if (args.size() < 2) throw std::runtime_error("mse() requires predicted and target tensors");
    
    Tensor* predicted = reinterpret_cast<Tensor*>(args[0].asInt());
    Tensor* target = reinterpret_cast<Tensor*>(args[1].asInt());
    
    if (!predicted || !target) throw std::runtime_error("Invalid tensor pointers");
    if (predicted->rows != target->rows || predicted->cols != target->cols) 
        throw std::runtime_error("Tensor shapes must match for MSE");
    
    double sum = 0.0;
    for (int i = 0; i < predicted->rows; i++) {
        for (int j = 0; j < predicted->cols; j++) {
            double diff = predicted->data[i][j] - target->data[i][j];
            sum += diff * diff;
        }
    }
    return Value(sum / (predicted->rows * predicted->cols));
}

Value native_cross_entropy(std::vector<Value>& args) {
    if (args.size() < 2) throw std::runtime_error("cross_entropy() requires predicted and target tensors");
    
    Tensor* predicted = reinterpret_cast<Tensor*>(args[0].asInt());
    Tensor* target = reinterpret_cast<Tensor*>(args[1].asInt());
    
    if (!predicted || !target) throw std::runtime_error("Invalid tensor pointers");
    if (predicted->rows != target->rows || predicted->cols != target->cols) 
        throw std::runtime_error("Tensor shapes must match for Cross-Entropy");
    
    double sum = 0.0;
    for (int i = 0; i < predicted->rows; i++) {
        for (int j = 0; j < predicted->cols; j++) {
            double p = std::max(predicted->data[i][j], 1e-15);
            sum += target->data[i][j] * std::log(p);
        }
    }
    return Value(-sum / predicted->rows);
}

Value native_mae(std::vector<Value>& args) {
    if (args.size() < 2) throw std::runtime_error("mae() requires predicted and target tensors");
    
    Tensor* predicted = reinterpret_cast<Tensor*>(args[0].asInt());
    Tensor* target = reinterpret_cast<Tensor*>(args[1].asInt());
    
    if (!predicted || !target) throw std::runtime_error("Invalid tensor pointers");
    if (predicted->rows != target->rows || predicted->cols != target->cols) 
        throw std::runtime_error("Tensor shapes must match for MAE");
    
    double sum = 0.0;
    for (int i = 0; i < predicted->rows; i++) {
        for (int j = 0; j < predicted->cols; j++) {
            sum += std::abs(predicted->data[i][j] - target->data[i][j]);
        }
    }
    return Value(sum / (predicted->rows * predicted->cols));
}

Value native_binary_cross_entropy(std::vector<Value>& args) {
    if (args.size() < 2) throw std::runtime_error("binary_cross_entropy() requires predicted and target");
    
    Tensor* predicted = reinterpret_cast<Tensor*>(args[0].asInt());
    Tensor* target = reinterpret_cast<Tensor*>(args[1].asInt());
    
    if (!predicted || !target) throw std::runtime_error("Invalid tensor pointers");
    if (predicted->rows != target->rows || predicted->cols != target->cols) 
        throw std::runtime_error("Tensor shapes must match");
    
    double sum = 0.0;
    for (int i = 0; i < predicted->rows; i++) {
        for (int j = 0; j < predicted->cols; j++) {
            double p = std::max(predicted->data[i][j], 1e-15);
            double t = target->data[i][j];
            sum += t * std::log(p) + (1 - t) * std::log(1 - p);
        }
    }
    return Value(-sum / (predicted->rows * predicted->cols));
}

} // namespace VMLNative