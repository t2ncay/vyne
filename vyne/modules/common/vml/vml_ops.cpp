#include "vml_common.h"

namespace VMLNative {

Value native_tensor_add(std::vector<Value>& args) {
    if (args.size() < 2) throw std::runtime_error("add() requires two tensors");
    
    Tensor* a = reinterpret_cast<Tensor*>(args[0].asInt());
    Tensor* b = reinterpret_cast<Tensor*>(args[1].asInt());
    
    if (!a || !b) throw std::runtime_error("Invalid tensor pointers");
    if (a->rows != b->rows || a->cols != b->cols) 
        throw std::runtime_error("Tensor shapes must match for addition");
    
    Tensor* result = new Tensor(a->rows, a->cols);
    for (int i = 0; i < a->rows; i++) {
        for (int j = 0; j < a->cols; j++) {
            result->data[i][j] = a->data[i][j] + b->data[i][j];
        }
    }
    return Value(reinterpret_cast<int64_t>(result));
}

Value native_tensor_sub(std::vector<Value>& args) {
    if (args.size() < 2) throw std::runtime_error("sub() requires two tensors");
    
    Tensor* a = reinterpret_cast<Tensor*>(args[0].asInt());
    Tensor* b = reinterpret_cast<Tensor*>(args[1].asInt());
    
    if (!a || !b) throw std::runtime_error("Invalid tensor pointers");
    if (a->rows != b->rows || a->cols != b->cols) 
        throw std::runtime_error("Tensor shapes must match for subtraction");
    
    Tensor* result = new Tensor(a->rows, a->cols);
    for (int i = 0; i < a->rows; i++) {
        for (int j = 0; j < a->cols; j++) {
            result->data[i][j] = a->data[i][j] - b->data[i][j];
        }
    }
    return Value(reinterpret_cast<int64_t>(result));
}

Value native_tensor_mul(std::vector<Value>& args) {
    if (args.size() < 2) throw std::runtime_error("mul() requires two tensors");
    
    Tensor* a = reinterpret_cast<Tensor*>(args[0].asInt());
    Tensor* b = reinterpret_cast<Tensor*>(args[1].asInt());
    
    if (!a || !b) throw std::runtime_error("Invalid tensor pointers");
    if (a->rows != b->rows || a->cols != b->cols) 
        throw std::runtime_error("Tensor shapes must match for element-wise multiplication");
    
    Tensor* result = new Tensor(a->rows, a->cols);
    for (int i = 0; i < a->rows; i++) {
        for (int j = 0; j < a->cols; j++) {
            result->data[i][j] = a->data[i][j] * b->data[i][j];
        }
    }
    return Value(reinterpret_cast<int64_t>(result));
}

Value native_tensor_div(std::vector<Value>& args) {
    if (args.size() < 2) throw std::runtime_error("div() requires two tensors");
    
    Tensor* a = reinterpret_cast<Tensor*>(args[0].asInt());
    Tensor* b = reinterpret_cast<Tensor*>(args[1].asInt());
    
    if (!a || !b) throw std::runtime_error("Invalid tensor pointers");
    if (a->rows != b->rows || a->cols != b->cols) 
        throw std::runtime_error("Tensor shapes must match for division");
    
    Tensor* result = new Tensor(a->rows, a->cols);
    for (int i = 0; i < a->rows; i++) {
        for (int j = 0; j < a->cols; j++) {
            if (b->data[i][j] == 0.0) throw std::runtime_error("Division by zero");
            result->data[i][j] = a->data[i][j] / b->data[i][j];
        }
    }
    return Value(reinterpret_cast<int64_t>(result));
}

Value native_tensor_matmul(std::vector<Value>& args) {
    if (args.size() < 2) throw std::runtime_error("matmul() requires two tensors");
    
    Tensor* a = reinterpret_cast<Tensor*>(args[0].asInt());
    Tensor* b = reinterpret_cast<Tensor*>(args[1].asInt());
    
    if (!a || !b) throw std::runtime_error("Invalid tensor pointers");
    if (a->cols != b->rows) 
        throw std::runtime_error("Matrix multiplication requires a->cols == b->rows");
    
    Tensor* result = new Tensor(a->rows, b->cols);
    for (int i = 0; i < a->rows; i++) {
        for (int j = 0; j < b->cols; j++) {
            double sum = 0.0;
            for (int k = 0; k < a->cols; k++) {
                sum += a->data[i][k] * b->data[k][j];
            }
            result->data[i][j] = sum;
        }
    }
    return Value(reinterpret_cast<int64_t>(result));
}

Value native_tensor_transpose(std::vector<Value>& args) {
    if (args.empty()) throw std::runtime_error("transpose() requires a tensor");
    
    Tensor* a = reinterpret_cast<Tensor*>(args[0].asInt());
    if (!a) throw std::runtime_error("Invalid tensor pointer");
    
    Tensor* result = new Tensor(a->cols, a->rows);
    for (int i = 0; i < a->rows; i++) {
        for (int j = 0; j < a->cols; j++) {
            result->data[j][i] = a->data[i][j];
        }
    }
    return Value(reinterpret_cast<int64_t>(result));
}

Value native_tensor_reshape(std::vector<Value>& args) {
    if (args.size() < 2) throw std::runtime_error("reshape() requires tensor and new size");
    
    Tensor* a = reinterpret_cast<Tensor*>(args[0].asInt());
    int new_size = (int)args[1].asInt();
    
    if (!a) throw std::runtime_error("Invalid tensor pointer");
    if (a->size() != new_size) throw std::runtime_error("Total size must match for reshape");
    
    // Flatten and reshape (simplified - assumes 1D or 2D)
    std::vector<double> flat;
    for (auto& row : a->data) {
        flat.insert(flat.end(), row.begin(), row.end());
    }
    
    Tensor* result = new Tensor(1, new_size);
    for (int i = 0; i < new_size; i++) {
        result->data[0][i] = flat[i];
    }
    return Value(reinterpret_cast<int64_t>(result));
}

Value native_tensor_sum(std::vector<Value>& args) {
    if (args.empty()) throw std::runtime_error("sum() requires a tensor");
    
    Tensor* a = reinterpret_cast<Tensor*>(args[0].asInt());
    if (!a) throw std::runtime_error("Invalid tensor pointer");
    
    double sum = 0.0;
    for (auto& row : a->data) {
        for (auto& v : row) sum += v;
    }
    return Value(sum);
}

Value native_tensor_mean(std::vector<Value>& args) {
    if (args.empty()) throw std::runtime_error("mean() requires a tensor");
    
    Tensor* a = reinterpret_cast<Tensor*>(args[0].asInt());
    if (!a) throw std::runtime_error("Invalid tensor pointer");
    
    double sum = 0.0;
    for (auto& row : a->data) {
        for (auto& v : row) sum += v;
    }
    return Value(sum / a->size());
}

Value native_tensor_max(std::vector<Value>& args) {
    if (args.empty()) throw std::runtime_error("max() requires a tensor");
    
    Tensor* a = reinterpret_cast<Tensor*>(args[0].asInt());
    if (!a) throw std::runtime_error("Invalid tensor pointer");
    
    double max_val = a->data[0][0];
    for (auto& row : a->data) {
        for (auto& v : row) {
            if (v > max_val) max_val = v;
        }
    }
    return Value(max_val);
}

Value native_tensor_min(std::vector<Value>& args) {
    if (args.empty()) throw std::runtime_error("min() requires a tensor");
    
    Tensor* a = reinterpret_cast<Tensor*>(args[0].asInt());
    if (!a) throw std::runtime_error("Invalid tensor pointer");
    
    double min_val = a->data[0][0];
    for (auto& row : a->data) {
        for (auto& v : row) {
            if (v < min_val) min_val = v;
        }
    }
    return Value(min_val);
}

} // namespace VMLNative