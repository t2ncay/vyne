# Algorithmic Feedback Delay Network (FDN) Reverb Architecture & Implementation

An **algorithmic reverb processor** simulates the acoustic reverberation of physical spaces by constructing a dense network of multi-stage diffusion allpass filters, modulated delay lines, and orthogonal feedback loops. Based on the classic **Dattorro / FDN** topology, this architecture provides high echo density, rich stereo width, and natural high-frequency damping.

---

## 1. System Architecture & Processing Topology

The reverb engine consists of four primary processing stages operating in series and feedback loops:

Input x[n] ---> [ Predelay ] ---> [ Input Diffusers ] ---> [ Feedback Matrix Loop ] ---> Output y[n]
| ^
v |
[ Damping & AP ] --+

1. **Predelay Line**: Delays the dry input signal to simulate initial acoustic wave propagation before room reflections.
2. **Input Diffuser Series**: A chain of 4 series allpass filters that smears sharp transients into dense initial reflection clusters.
3. **Feedback Matrix Loop**: A 4-channel feedback network using a Householder feedback matrix to generate late reverberation tails.
4. **Recirculating Processing**: Integrates high-frequency one-pole damping filters, subtle soft-clipping saturation ($\tanh$), in-loop diffusion, and multi-rate LFO delay modulation to eliminate standing waves and metallic resonances.

---

## 2. Mathematical Foundation

### A. One-Pole Lowpass Damping Filter

Lowpass damping in the feedback loop models acoustic high-frequency air absorption and surface materials:

$$y[n] = x[n] \cdot (1 - \beta) + y[n-1] \cdot \beta$$

Where $\beta$ is the damping coefficient controlled by the user ($0.10 \le \beta \le 0.92$).

### B. Allpass Diffuser Filter

First-order lattice allpass diffusion disperses transient energy without altering overall frequency magnitude:

$$w[n] = x[n] + f \cdot w[n-1]$$

$$y[n] = -x[n] + (1 - f^2) \cdot w[n-1]$$

In direct Form, with feedback coefficient $f = 0.65$:

$$y[n] = -x[n] + y_{\text{buf}}[n-1]$$

$$y_{\text{buf}}[n] = x[n] + f \cdot y_{\text{buf}}[n-1]$$

### C. Modulated Delay Line Interpolation

Continuous pitch-free modulation avoids metallic flanging by applying fractional delay reading via linear interpolation:

$$\text{delay}[n] = d_{\text{base}} + d_{\text{depth}} \cdot \sin(\phi[n])$$

$$\phi[n] = (\phi[n-1] + \Delta\phi) \pmod{2\pi}, \quad \Delta\phi = \frac{2\pi \cdot f_{\text{mod}}}{F_s}$$

$$y[n] = x[\text{writeIdx} - \lfloor\text{delay}\rfloor] \cdot (1 - \text{frac}) + x[\text{writeIdx} - \lfloor\text{delay}\rfloor - 1] \cdot \text{frac}$$

### D. Householder Feedback Scattering Matrix

Inter-channel feedback mixing uses an orthogonal Householder transformation to ensure maximum reflection density while maintaining energy conservation:

$$S[n] = \frac{1}{2} \sum_{k=0}^{3} v_k[n]$$

$$u_j[n] = v_j[n] + g \cdot (S[n] - v_j[n])$$

Where $g$ is the global decay gain factor ($0.70 \le g \le 0.985$).

### E. Equal-Power Constant Gain Crossfade

To preserve total signal energy across dry and wet signal adjustments, crossfading uses trigonometric equal-power curves:

$$g_{\text{wet}} = \sin\left(\frac{\pi}{2} \cdot \text{Mix}\right)$$

$$g_{\text{dry}} = \cos\left(\frac{\pi}{2} \cdot \text{Mix}\right)$$

$$y_{\text{final}}[n] = x[n] \cdot g_{\text{dry}} + y_{\text{wet}}[n] \cdot g_{\text{wet}}$$

---
