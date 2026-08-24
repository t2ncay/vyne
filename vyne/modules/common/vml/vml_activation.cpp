#include "vml_common.h"

namespace VMLNative {

// DenseLayer already has activation built-in, but we keep this for standalone activation functions

// Activation functions (can be called directly from Vyne)
Value native_relu(std::vector<Value>& args) {
    if (args.empty()) throw std::runtime_error("relu() requires a tensor");
    
    Tensor* a = reinterpret_cast<Tensor*>(args[0].asInt());
    if (!a) throw std::runtime_error("Invalid tensor pointer");
    
    Tensor* result = new Tensor(a->data);
    for (auto& row : result->data) {
        for (auto& v : row) v = (v > 0) ? v : 0.0;
    }
    return Value(reinterpret_cast<int64_t>(result));
}

Value native_sigmoid(std::vector<Value>& args) {
    if (args.empty()) throw std::runtime_error("sigmoid() requires a tensor");
    
    Tensor* a = reinterpret_cast<Tensor*>(args[0].asInt());
    if (!a) throw std::runtime_error("Invalid tensor pointer");
    
    Tensor* result = new Tensor(a->data);
    for (auto& row : result->data) {
        for (auto& v : row) v = 1.0 / (1.0 + std::exp(-v));
    }
    return Value(reinterpret_cast<int64_t>(result));
}

Value native_tanh(std::vector<Value>& args) {
    if (args.empty()) throw std::runtime_error("tanh() requires a tensor");
    
    Tensor* a = reinterpret_cast<Tensor*>(args[0].asInt());
    if (!a) throw std::runtime_error("Invalid tensor pointer");
    
    Tensor* result = new Tensor(a->data);
    for (auto& row : result->data) {
        for (auto& v : row) v = std::tanh(v);
    }
    return Value(reinterpret_cast<int64_t>(result));
}

Value native_softmax(std::vector<Value>& args) {
    if (args.empty()) throw std::runtime_error("softmax() requires a tensor");
    
    Tensor* a = reinterpret_cast<Tensor*>(args[0].asInt());
    if (!a) throw std::runtime_error("Invalid tensor pointer");
    
    Tensor* result = new Tensor(a->data);
    for (int i = 0; i < result->rows; i++) {
        double max_val = *std::max_element(result->data[i].begin(), result->data[i].end());
        double sum = 0.0;
        for (auto& v : result->data[i]) {
            v = std::exp(v - max_val);
            sum += v;
        }
        for (auto& v : result->data[i]) v /= sum;
    }
    return Value(reinterpret_cast<int64_t>(result));
}

} // namespace VMLNative