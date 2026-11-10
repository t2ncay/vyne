# train.vy - train the residual SRCNN on the generated dataset.
#
#   vynec --compile src/train.vy && ./src/train
#
# The forward pass uses vconv's native convolution kernels. The backward
# pass uses the explicit vconv backward kernels and voptim Adam. This is
# the honest path today: @differentiable has no convolution VJP and no
# array-local support, and @device exposes matmul/elementwise, not conv.

use native vcore;
use native vmath;
use external "vimage/vimage.vy";
use external "vconv/vconv.vy";
use external "voptim/voptim.vy";
use "model.vy";
use "metrics.vy";

out("=== srcnn training ===");

in_img :: vimage.Image := vimage.load("data/train_input.png");
hr_img :: vimage.Image := vimage.load("data/train_hr.png");
if in_img.width != hr_img.width || in_img.height != hr_img.height {
    throw "train: input and target dimensions differ";
}

x :: vconv.Tensor := vconv.tensor(in_img.pixels, in_img.height, in_img.width, in_img.channels);
y :: vconv.Tensor := vconv.tensor(hr_img.pixels, hr_img.height, hr_img.width, hr_img.channels);

model :: SRCNN := init_model();

# Adam states point at the model's own arrays, so updates are in place.
z_w1 :: Array<Float64> := zeros(8 * 3 * 5 * 5);
v_w1 :: Array<Float64> := zeros(8 * 3 * 5 * 5);
z_b1 :: Array<Float64> := zeros(8);
v_b1 :: Array<Float64> := zeros(8);
z_w2 :: Array<Float64> := zeros(4 * 8);
v_w2 :: Array<Float64> := zeros(4 * 8);
z_b2 :: Array<Float64> := zeros(4);
v_b2 :: Array<Float64> := zeros(4);
z_w3 :: Array<Float64> := zeros(3 * 4 * 5 * 5);
v_w3 :: Array<Float64> := zeros(3 * 4 * 5 * 5);
z_b3 :: Array<Float64> := zeros(3);
v_b3 :: Array<Float64> := zeros(3);

opt_w1 :: voptim.AdamState := voptim.AdamState(model.w1, z_w1, v_w1, 0.9, 0.999, 1e-8, 0.0, 0, 8 * 3 * 5 * 5);
opt_b1 :: voptim.AdamState := voptim.AdamState(model.b1, z_b1, v_b1, 0.9, 0.999, 1e-8, 0.0, 0, 8);
opt_w2 :: voptim.AdamState := voptim.AdamState(model.w2, z_w2, v_w2, 0.9, 0.999, 1e-8, 0.0, 0, 4 * 8);
opt_b2 :: voptim.AdamState := voptim.AdamState(model.b2, z_b2, v_b2, 0.9, 0.999, 1e-8, 0.0, 0, 4);
opt_w3 :: voptim.AdamState := voptim.AdamState(model.w3, z_w3, v_w3, 0.9, 0.999, 1e-8, 0.0, 0, 3 * 4 * 5 * 5);
opt_b3 :: voptim.AdamState := voptim.AdamState(model.b3, z_b3, v_b3, 0.9, 0.999, 1e-8, 0.0, 0, 3);

# Initial loss is exactly the bilinear baseline because the residual head
# starts at zero.
initial :: vconv.Tensor := forward(model, x);
initial_img :: vimage.Image := vimage.Image(initial.width, initial.height, initial.channels, initial.data);
out("initial PSNR (bilinear): " + string(psnr(initial_img, hr_img)) + " dB");
initial_loss :: Grads := loss_and_grads(model, x, y);
out("initial L2             : " + string(initial_loss.loss));

EPOCHS :: Int64 := 300;
BASE_LR :: Float64 := 0.002;

t0 :: Int64 := vcore.now_ns();
through epoch :: 1..EPOCHS -> loop {
    g :: Grads := loss_and_grads(model, x, y);
    lr :: Float64 := voptim.cosine(epoch, BASE_LR, EPOCHS);

    voptim.adam_step(opt_w1, voptim.Series(g.w1, 8 * 3 * 5 * 5), lr);
    voptim.adam_step(opt_b1, voptim.Series(g.b1, 8), lr);
    voptim.adam_step(opt_w2, voptim.Series(g.w2, 4 * 8), lr);
    voptim.adam_step(opt_b2, voptim.Series(g.b2, 4), lr);
    voptim.adam_step(opt_w3, voptim.Series(g.w3, 3 * 4 * 5 * 5), lr);
    voptim.adam_step(opt_b3, voptim.Series(g.b3, 3), lr);

    if (epoch % 5 == 0) || (epoch == 1) {
        out("epoch " + string(epoch) + " | lr " + string(lr) + " | L2 " + string(g.loss));
    }
};
t1 :: Int64 := vcore.now_ns();

final :: vconv.Tensor := forward(model, x);
final_img :: vimage.Image := vimage.Image(final.width, final.height, final.channels, final.data);
out("final PSNR              : " + string(psnr(final_img, hr_img)) + " dB");
out("training time           : " + string((t1 - t0) / 1000000) + " ms");

save_model("data/srcnn_x2.vwt", model);
out("saved data/srcnn_x2.vwt");
out("=== done ===");