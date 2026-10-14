# vml/Types.vy — the ML interface surface.
#
# Everything here is built on top of vlin.Types.Matrix. Dense layers
# store a weight Matrix, a bias Array<Float64>, and the name of their
# activation. Sequential composes layers. SGD carries a learning rate.
# All numeric work is delegated to vlin.
ruleset { dynamic_casting };

use native vmath;
use external "vlin/vlin.vy";

module vml;

group Types :: vml {

    interface Dense {
        W          :: vlin.Types.Matrix,
        b          :: Array<Float64>,
        activation :: String,

        in_features()  -> Int64 { return self.W.row; }
        out_features() -> Int64 { return self.W.col; }

        # z = x @ W + b; then activation(z). Dispatches on the
        # activation string via vml.apply_activation in Ops.vy.
        forward(x :: vlin.Types.Matrix) -> vlin.Types.Matrix {
            z :: vlin.Types.Matrix :=
                vlin.add_bias(vlin.multiply(x, self.W), self.b);
            return vml.apply_activation(z, self.activation);
        }

        # Derivative of the activation, evaluated at the post-
        # activation output `a`.
        prime(a :: vlin.Types.Matrix) -> vlin.Types.Matrix {
            return vml.apply_activation_prime(a, self.activation);
        }
    }

    interface Sequential {
        layers :: Array,

        depth() -> Int64 { return self.layers.size(); }

        forward(x :: vlin.Types.Matrix) -> vlin.Types.Matrix {
            result :: vlin.Types.Matrix := x;
            n :: Int64 := self.layers.size();
            through i :: 0..n-1 -> loop {
                layer := self.layers[i];
                result = layer.forward(result);
            };
            return result;
        }
    }

    interface SGD {
        lr :: Float64,
        lr_value() -> Float64 { return self.lr; }
    }

    # Adam hyperparameters. The per-parameter state lives in a
    # separate AdamState value — one per weight matrix — because Vyne
    # has no way to attach auxiliary storage to an existing interface
    # value without changing its native constructor ABI.
    interface Adam {
        lr    :: Float64,
        beta1 :: Float64,
        beta2 :: Float64,
        eps   :: Float64,
    }

    # Per-weight state. m and v are the first and second moment
    # estimates, both flat Array<Float64> of length W.row * W.col.
    interface AdamState {
        m :: Array<Float64>,
        v :: Array<Float64>,
    }
}
