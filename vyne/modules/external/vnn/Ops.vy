# vnn/Ops.vy - @device_fn neural-network building blocks.

use "Types.vy";
use external "vtensor/vtensor.vy";

ruleset { dynamic_casting };

module vnn;

fn :: vnn adamw_hyper(lr :: Float64, beta1 :: Float64, beta2 :: Float64,
                      eps :: Float64, weight_decay :: Float64) -> vnn.AdamWHyper {
    return vnn.AdamWHyper(lr, beta1, beta2, eps, weight_decay);
}

@device_fn
fn :: vnn conv2d(dst :: vtensor.DeviceTensor,
                 input :: vtensor.DeviceTensor,
                 weight :: Int64,
                 bias :: Int64,
                 out_c :: Int64,
                 kernel :: Int64) {
    device_conv2d(dst.handle, input.handle, weight, bias,
        input.height, input.width, input.channels,
        dst.height, dst.width, out_c, kernel, 1, kernel / 2, 1);
}

@device_fn
fn :: vnn conv2d_relu(pre :: vtensor.DeviceTensor,
                      act :: vtensor.DeviceTensor,
                      input :: vtensor.DeviceTensor,
                      weight :: Int64,
                      bias :: Int64,
                      out_c :: Int64,
                      kernel :: Int64) {
    vnn.conv2d(pre, input, weight, bias, out_c, kernel);
    device_relu(act.handle, pre.handle, act.n);
}

@device_fn
fn :: vnn pixel_shuffle(dst :: vtensor.DeviceTensor,
                        input :: vtensor.DeviceTensor,
                        scale :: Int64) {
    device_pixel_shuffle(dst.handle, input.handle,
        input.height, input.width, input.channels, scale);
}

@device_fn
fn :: vnn mse_grad_sum(grad :: Int64,
                       partial :: Int64,
                       loss :: Int64,
                       pred :: vtensor.DeviceTensor,
                       target :: vtensor.DeviceTensor) {
    device_mse_grad(grad, partial, pred.handle, target.handle, pred.n);
    device_sum(loss, partial, pred.n);
}

@device_fn
fn :: vnn pixel_shuffle_backward(grad_in :: vtensor.DeviceTensor,
                                 grad_out :: vtensor.DeviceTensor,
                                 scale :: Int64) {
    device_pixel_shuffle_backward(grad_in.handle, grad_out.handle,
        grad_in.height, grad_in.width, grad_in.channels, scale);
}

@device_fn
fn :: vnn conv2d_backward(grad_input :: vtensor.DeviceTensor,
                          grad_weight :: Int64,
                          grad_bias :: Int64,
                          input :: vtensor.DeviceTensor,
                          grad_out :: vtensor.DeviceTensor,
                          weight :: Int64,
                          out_c :: Int64,
                          kernel :: Int64) {
    device_conv2d_backward_weight(grad_weight, input.handle, grad_out.handle,
        input.height, input.width, input.channels,
        grad_out.height, grad_out.width, out_c, kernel, 1, kernel / 2);
    device_conv2d_backward_bias(grad_bias, grad_out.handle,
        grad_out.height, grad_out.width, out_c);
    device_conv2d_backward_input(grad_input.handle, grad_out.handle, weight,
        input.height, input.width, input.channels,
        grad_out.height, grad_out.width, out_c, kernel, 1, kernel / 2);
}

@device_fn
fn :: vnn conv2d_relu_backward(grad_input :: vtensor.DeviceTensor,
                               grad_weight :: Int64,
                               grad_bias :: Int64,
                               grad_pre :: vtensor.DeviceTensor,
                               input :: vtensor.DeviceTensor,
                               grad_act :: vtensor.DeviceTensor,
                               pre :: vtensor.DeviceTensor,
                               weight :: Int64,
                               out_c :: Int64,
                               kernel :: Int64) {
    device_relu_grad(grad_pre.handle, grad_act.handle, pre.handle, pre.n);
    vnn.conv2d_backward(grad_input, grad_weight, grad_bias, input,
        grad_pre, weight, out_c, kernel);
}

@device_fn
fn :: vnn adamw(param :: Int64,
                m :: Int64,
                v :: Int64,
                grad :: Int64,
                hyper :: vnn.AdamWHyper,
                step :: Int64,
                n :: Int64) {
    device_adamw_step(param, m, v, grad, hyper.lr, hyper.beta1, hyper.beta2,
        hyper.eps, hyper.weight_decay, step, n);
}
