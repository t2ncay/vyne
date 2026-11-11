# device_optim.vy - project-local optimizer wrappers.

use "model.vy";

@device_fn
fn adamw_step_espcn_device(
    model :: ESPCN,
    step :: Int64,
    lr :: Float64,
    beta1 :: Float64,
    beta2 :: Float64,
    eps :: Float64,
    weight_decay :: Float64,
    d_w1 :: Int64, d_m_w1 :: Int64, d_v_w1 :: Int64, d_gw1 :: Int64, w1_n :: Int64,
    d_b1 :: Int64, d_m_b1 :: Int64, d_v_b1 :: Int64, d_gb1 :: Int64,
    d_w2 :: Int64, d_m_w2 :: Int64, d_v_w2 :: Int64, d_gw2 :: Int64, w2_n :: Int64,
    d_b2 :: Int64, d_m_b2 :: Int64, d_v_b2 :: Int64, d_gb2 :: Int64,
    d_w3 :: Int64, d_m_w3 :: Int64, d_v_w3 :: Int64, d_gw3 :: Int64, w3_n :: Int64,
    d_b3 :: Int64, d_m_b3 :: Int64, d_v_b3 :: Int64, d_gb3 :: Int64
) {
    device_adamw_step(d_w1, d_m_w1, d_v_w1, d_gw1, lr, beta1, beta2, eps, weight_decay, step, w1_n);
    device_adamw_step(d_b1, d_m_b1, d_v_b1, d_gb1, lr, beta1, beta2, eps, weight_decay, step, model.c1);
    device_adamw_step(d_w2, d_m_w2, d_v_w2, d_gw2, lr, beta1, beta2, eps, weight_decay, step, w2_n);
    device_adamw_step(d_b2, d_m_b2, d_v_b2, d_gb2, lr, beta1, beta2, eps, weight_decay, step, model.c2);
    device_adamw_step(d_w3, d_m_w3, d_v_w3, d_gw3, lr, beta1, beta2, eps, weight_decay, step, w3_n);
    device_adamw_step(d_b3, d_m_b3, d_v_b3, d_gb3, lr, beta1, beta2, eps, weight_decay, step, model.c3);
}
