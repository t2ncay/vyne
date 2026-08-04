# Dynamic Range Compressor Architecture & Implementation

A **dynamic range compressor** reduces the dynamic range of an audio signal by attenuating signals that exceed a specified threshold. It is a fundamental tool in audio engineering used for controlling transients, glueing mixes, and maximizing perceived loudness.

---

## 1. System Architecture & Processing Topology

The compressor system consists of four primary stages operating in sequence:

Input x[n] ---> [ Detection Stage ] ---> [ Envelope Follower ] ---> [ Gain Computer ] ---> [ Makeup Gain ] ---> Output y[n]
                       |                                                   |
                       +-------------------> (Sidechain Path) -------------+

1. **Detection Stage**: Measures signal amplitude using **Peak** or **RMS** (Root Mean Square) algorithms.
2. **Envelope Follower**: Applies time-constant filtering (**Attack** and **Release**) to smooth detection energy.
3. **Gain Computer**: Calculates gain reduction in decibels ($\text{dB}$) based on **Threshold** and **Ratio**.
4. **Makeup Gain**: Restores lost signal energy manually or automatically.

---

## 2. Mathematical Foundation

### A. Time-Constant Coefficients

Discrete-time single-pole IIR filter coefficients for Attack ($\alpha_a$), Release ($\alpha_r$), and RMS Window ($\alpha_{\text{rms}}$) are calculated from time parameters in milliseconds ($t_{\text{ms}}$) and sample rate ($F_s$):

$$\alpha = \exp\left(-\frac{1}{0.001 \cdot t_{\text{ms}} \cdot F_s}\right)$$

### B. Level Detection Modes

#### Peak Detection
Extracts absolute instantaneous peak amplitude from stereo channels:

$$x_{\text{peak}}[n] = \max(|x_L[n]|, |x_R[n]|)$$

#### RMS Detection
Calculates the smoothed root-mean-square energy over an integration window:

$$P[n] = 0.5 \cdot \left( x_L^2[n] + x_R^2[n] \right)$$

$$S_{\text{rms}}[n] = \alpha_{\text{rms}} \cdot S_{\text{rms}}[n-1] + (1 - \alpha_{\text{rms}}) \cdot P[n]$$

$$x_{\text{rms}}[n] = \sqrt{\max(S_{\text{rms}}[n], 10^{-12})}$$

### C. Envelope Follower (Branching Ballistics)

Applies decoupled attack and release time constants to smooth detector transitions:

$$
E[n] = 
\begin{cases} 
\alpha_a \cdot E[n-1] + (1 - \alpha_a) \cdot x_{\text{det}}[n], & \text{if } x_{\text{det}}[n] > E[n-1] \text{ (Attack)} \\
\alpha_r \cdot E[n-1] + (1 - \alpha_r) \cdot x_{\text{det}}[n], & \text{if } x_{\text{det}}[n] \le E[n-1] \text{ (Release)}
\end{cases}
$$

### D. Gain Computation (Static Characteristic)

Converts envelope amplitude to decibels, determines excess level above threshold $T_{\text{dB}}$, and computes attenuation $G_{\text{dB}}$:

$$E_{\text{dB}}[n] = 20 \cdot \log_{10}\left(\max(E[n], 10^{-6})\right)$$

$$\Delta_{\text{dB}}[n] = \max(E_{\text{dB}}[n] - T_{\text{dB}}, 0)$$

$$G_{\text{dB}}[n] = \Delta_{\text{dB}}[n] \cdot \left(1 - \frac{1}{R}\right)$$

Where:
- $T_{\text{dB}}$ is the **Threshold** in decibels.
- $R$ is the compression **Ratio** ($R : 1$).

### E. Auto Makeup Gain Compensation

Predicts average Gain Reduction ($GR$) to automatically normalize output levels:

$$\text{Gain}_{\text{auto\_dB}} = (-T_{\text{dB}}) \cdot \left(1 - \frac{1}{R}\right) \cdot 0.85$$

$$\text{Gain}_{\text{total}} = 10^{\frac{\text{Gain}_{\text{makeup\_dB}} + \text{Gain}_{\text{auto\_dB}}}{20}} \cdot 10^{-\frac{G_{\text{dB}}[n]}{20}}$$

---