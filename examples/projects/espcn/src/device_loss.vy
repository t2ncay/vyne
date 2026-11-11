# device_loss.vy - project-local device loss helpers.

@device_fn
fn mse_grad_sum_device(
    d_loss_grad :: Int64,
    d_loss_part :: Int64,
    d_loss :: Int64,
    d_pred :: Int64,
    d_target :: Int64,
    n :: Int64
) {
    device_mse_grad(d_loss_grad, d_loss_part, d_pred, d_target, n);
    device_sum(d_loss, d_loss_part, n);
}
