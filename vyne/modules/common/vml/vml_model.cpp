#include "vml_common.h"

namespace VMLNative {

Value native_sequential(std::vector<Value>& args) {
    std::string name = (args.empty()) ? "model" : args[0].asString();
    Sequential* model = new Sequential(name);
    return Value(reinterpret_cast<int64_t>(model));
}

Value native_model_add(std::vector<Value>& args) {
    if (args.size() < 2) throw std::runtime_error("model.add() requires model and layer");
    
    Sequential* model = reinterpret_cast<Sequential*>(args[0].asInt());
    Layer* layer = reinterpret_cast<Layer*>(args[1].asInt());
    
    if (model && layer) {
        model->add(layer);
        return Value(true);
    }
    return Value(false);
}

Value native_model_train(std::vector<Value>& args) {
    if (args.size() < 3) throw std::runtime_error("train() requires model, input, target");
    
    Sequential* model = reinterpret_cast<Sequential*>(args[0].asInt());
    Tensor* input = reinterpret_cast<Tensor*>(args[1].asInt());
    Tensor* target = reinterpret_cast<Tensor*>(args[2].asInt());
    
    if (!model || !input || !target) throw std::runtime_error("Invalid pointers");
    
    int epochs = (args.size() > 3) ? (int)args[3].asInt() : 100;
    bool verbose = (args.size() > 4) ? args[4].isTruthy() : true;
    double lr = (args.size() > 5) ? args[5].asFloat() : 0.001;
    
    SGDOptimizer optimizer(lr);
    model->train(input, target, optimizer, epochs, verbose);
    return Value(true);
}

Value native_model_predict(std::vector<Value>& args) {
    if (args.size() < 2) throw std::runtime_error("predict() requires model and input");
    
    Sequential* model = reinterpret_cast<Sequential*>(args[0].asInt());
    Tensor* input = reinterpret_cast<Tensor*>(args[1].asInt());
    
    if (!model || !input) throw std::runtime_error("Invalid pointers");
    
    Tensor* output = model->predict(input);
    return Value(reinterpret_cast<int64_t>(output));
}

Value native_model_evaluate(std::vector<Value>& args) {
    if (args.size() < 3) throw std::runtime_error("evaluate() requires model, input, target");
    
    Sequential* model = reinterpret_cast<Sequential*>(args[0].asInt());
    Tensor* input = reinterpret_cast<Tensor*>(args[1].asInt());
    Tensor* target = reinterpret_cast<Tensor*>(args[2].asInt());
    
    if (!model || !input || !target) throw std::runtime_error("Invalid pointers");
    
    Tensor* output = model->predict(input);
    double loss = Loss::mse(output, target);
    delete output;
    return Value(loss);
}

Value native_model_summary(std::vector<Value>& args) {
    if (args.empty()) throw std::runtime_error("summary() requires a model");
    
    Sequential* model = reinterpret_cast<Sequential*>(args[0].asInt());
    if (!model) throw std::runtime_error("Invalid model pointer");
    
    std::cout << "\n========================================\n";
    std::cout << "  Model: " << model->name << "\n";
    std::cout << "========================================\n";
    std::cout << "  Layer (type)         Output Shape    Parameters\n";
    std::cout << "----------------------------------------\n";
    
    int total_params = 0;
    for (auto* layer : model->layers) {
        auto params = layer->parameters();
        int layer_params = 0;
        for (auto* p : params) {
            layer_params += p->size();
        }
        total_params += layer_params;
        
        std::cout << "  " << layer->name << " (";
        if (dynamic_cast<DenseLayer*>(layer)) std::cout << "Dense";
        else if (dynamic_cast<DropoutLayer*>(layer)) std::cout << "Dropout";
        else if (dynamic_cast<ActivationLayer*>(layer)) std::cout << "Activation";
        else std::cout << "Custom";
        std::cout << ")                ";
        std::cout << " -              " << layer_params << "\n";
    }
    
    std::cout << "----------------------------------------\n";
    std::cout << "  Total parameters: " << total_params << "\n";
    std::cout << "========================================\n\n";
    
    return Value(true);
}

} // namespace VMLNative