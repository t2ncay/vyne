# vnn/Types.vy - device neural-network helper types.

ruleset { dynamic_casting };

module vnn;

interface AdamWHyper {
    lr           :: Float64,
    beta1        :: Float64,
    beta2        :: Float64,
    eps          :: Float64,
    weight_decay :: Float64,
}
