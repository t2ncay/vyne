#include "vml.h"

void setupVML(SymbolContainer& env, StringPool& pool) {
    const std::string& path = "vml";
    
    if (env.find(path) == env.end()) {
        env[path] = SymbolTable();
    }
    
    auto& vml = env[path];
    
    // Tensor creation
    vml[pool.intern("tensor")] = Value(VMLNative::native_tensor);
    vml[pool.intern("zeros")] = Value(VMLNative::native_tensor_zeros);
    vml[pool.intern("ones")] = Value(VMLNative::native_tensor_ones);
    vml[pool.intern("random")] = Value(VMLNative::native_tensor_random);
    vml[pool.intern("eye")] = Value(VMLNative::native_tensor_eye);
    
    // Tensor operations
    vml[pool.intern("add")] = Value(VMLNative::native_tensor_add);
    vml[pool.intern("sub")] = Value(VMLNative::native_tensor_sub);
    vml[pool.intern("mul")] = Value(VMLNative::native_tensor_mul);
    vml[pool.intern("div")] = Value(VMLNative::native_tensor_div);
    vml[pool.intern("matmul")] = Value(VMLNative::native_tensor_matmul);
    vml[pool.intern("transpose")] = Value(VMLNative::native_tensor_transpose);
    vml[pool.intern("reshape")] = Value(VMLNative::native_tensor_reshape);
    vml[pool.intern("sum")] = Value(VMLNative::native_tensor_sum);
    vml[pool.intern("mean")] = Value(VMLNative::native_tensor_mean);
    vml[pool.intern("max")] = Value(VMLNative::native_tensor_max);
    vml[pool.intern("min")] = Value(VMLNative::native_tensor_min);
    
    // Layers
    vml[pool.intern("dense")] = Value(VMLNative::native_dense);
    vml[pool.intern("activation")] = Value(VMLNative::native_activation);
    vml[pool.intern("dropout")] = Value(VMLNative::native_dropout);
    vml[pool.intern("flatten")] = Value(VMLNative::native_flatten);
    vml[pool.intern("reshape_layer")] = Value(VMLNative::native_reshape_layer);
    
    // Model
    vml[pool.intern("sequential")] = Value(VMLNative::native_sequential);
    vml[pool.intern("add_layer")] = Value(VMLNative::native_model_add);
    vml[pool.intern("train")] = Value(VMLNative::native_model_train);
    vml[pool.intern("predict")] = Value(VMLNative::native_model_predict);
    vml[pool.intern("evaluate")] = Value(VMLNative::native_model_evaluate);
    vml[pool.intern("summary")] = Value(VMLNative::native_model_summary);
    
    // Loss functions
    vml[pool.intern("mse")] = Value(VMLNative::native_mse);
    vml[pool.intern("cross_entropy")] = Value(VMLNative::native_cross_entropy);
    vml[pool.intern("mae")] = Value(VMLNative::native_mae);
    vml[pool.intern("binary_cross_entropy")] = Value(VMLNative::native_binary_cross_entropy);
    
    // Optimizers
    vml[pool.intern("sgd")] = Value(VMLNative::native_sgd);
    vml[pool.intern("adam")] = Value(VMLNative::native_adam);
    vml[pool.intern("rmsprop")] = Value(VMLNative::native_rmsprop);
    
    // Constants
    vml[pool.intern("version")] = Value("v0.1.0-alpha").setReadOnly();
    vml[pool.intern("train_mode")] = Value(1).setReadOnly();
    vml[pool.intern("eval_mode")] = Value(0).setReadOnly();
}