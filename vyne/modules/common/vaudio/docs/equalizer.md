# Biquad Equalizer Architecture & Implementation

A **biquad filter** (short for _biquadratic filter_) is a second-order Infinite Impulse Response (IIR) digital filter. It contains two poles and two zeros, making it the fundamental building block for audio equalizers, parametric EQs, and crossover networks.

---

## 1. Z-Domain Transfer Function

The transfer function of a biquad filter in the $z$-domain is represented as a ratio of two quadratic polynomials:

$$H(z) = \frac{Y(z)}{X(z)} = \frac{b_0 + b_1 z^{-1} + b_2 z^{-2}}{a_0 + a_1 z^{-1} + a_2 z^{-2}}$$

Where:

- $X(z)$ is the filter input.
- $Y(z)$ is the filter output.
- $b_0, b_1, b_2$ are the **feedforward coefficients** (zeros).
- $a_0, a_1, a_2$ are the **feedback coefficients** (poles).

Normalizing all coefficients by dividing by $a_0$ yields the standardized form ($a_0 = 1$):

$$H(z) = \frac{b_0 + b_1 z^{-1} + b_2 z^{-2}}{1 + a_1 z^{-1} + a_2 z^{-2}}$$

---

## 2. Time-Domain Realization (Direct Form II Transposed)

While Direct Form I (DF1) uses four delay state registers ($x[n-1], x[n-2], y[n-1], y[n-2]$), **Direct Form II Transposed (DF2T)** requires only two memory states ($z_1, z_2$).

DF2T is the preferred topology in real-time audio systems due to improved numerical stability and lower CPU cache footprints.

```text
Input x[n] ---> [ b0 ] ----------------------(+)---> Output y[n]
|                           |
(+) <--- [ -a1 ] <--- [z^-1] (z1)
|                     |
[ b1 ]                 (+)
|                     |
[ b2 ] ---> [ -a2 ] -> [z^-1] (z2)
```

### Difference Equations

For each sample $n$:

$$y[n] = b_0 x[n] + z_1[n-1]$$

$$z_1[n] = b_1 x[n] - a_1 y[n] + z_2[n-1]$$

$$z_2[n] = b_2 x[n] - a_2 y[n]$$

---

## 3. RBJ Cookbook Coefficient Calculations

Coefficient generation follows the standard **Robert Bristow-Johnson (RBJ) Audio EQ Cookbook** formulas.

### Common Intermediate Variables

Given parameters:

- $f_0$: Center or cutoff frequency ($\text{Hz}$)
- $F_s$: System sample rate ($\text{Hz}$, e.g., $48000\text{ Hz}$)
- $Q$: Quality factor / Resonance ($Q = \frac{f_0}{\text{BW}}$)
- $\text{Gain}_{\text{dB}}$: Band gain in decibels ($\text{dB}$)

$$\omega_0 = 2\pi \frac{f_0}{F_s}$$

$$\alpha = \frac{\sin(\omega_0)}{2Q}$$

$$A = 10^{\frac{\text{Gain}_{\text{dB}}}{40}} = \sqrt{10^{\frac{\text{Gain}_{\text{dB}}}{20}}}$$

---

### A. Peaking Bell Filter

Used for mid-range adjustments (boosting or cutting specific frequencies).

$$
\begin{aligned}
b_0 &= 1 + \alpha A & a_0 &= 1 + \frac{\alpha}{A} \\
b_1 &= -2\cos(\omega_0) & a_1 &= -2\cos(\omega_0) \\
b_2 &= 1 - \alpha A & a_2 &= 1 - \frac{\alpha}{A}
\end{aligned}
$$

Normalize all parameters by dividing by $a_0$:

$$b_0 \leftarrow \frac{b_0}{a_0}, \quad b_1 \leftarrow \frac{b_1}{a_0}, \quad b_2 \leftarrow \frac{b_2}{a_0}, \quad a_1 \leftarrow \frac{a_1}{a_0}, \quad a_2 \leftarrow \frac{a_2}{a_0}$$

---

### B. High-Pass Filter (HPF - 12 dB/octave)

Attenuates frequencies below cutoff $f_0$.

$$
\begin{aligned}
b_0 &= \frac{1 + \cos(\omega_0)}{2} & a_0 &= 1 + \alpha \\
b_1 &= -(1 + \cos(\omega_0)) & a_1 &= -2\cos(\omega_0) \\
b_2 &= \frac{1 + \cos(\omega_0)}{2} & a_2 &= 1 - \alpha
\end{aligned}
$$

Normalize all parameters by $a_0$.

---

### C. High Shelf Filter

Boosts or cuts all frequencies above $f_0$.

$$
\begin{aligned}
b_0 &= A \left( (A+1) + (A-1)\cos(\omega_0) + 2\sqrt{A}\alpha \right) \\
b_1 &= -2A \left( (A-1) + (A+1)\cos(\omega_0) \right) \\
b_2 &= A \left( (A+1) + (A-1)\cos(\omega_0) - 2\sqrt{A}\alpha \right) \\
a_0 &= (A+1) - (A-1)\cos(\omega_0) + 2\sqrt{A}\alpha \\
a_1 &= 2 \left( (A-1) - (A+1)\cos(\omega_0) \right) \\
a_2 &= (A+1) - (A-1)\cos(\omega_0) - 2\sqrt{A}\alpha
\end{aligned}
$$

Normalize all parameters by $a_0$.

---

## 4. Plotting Frequency Response (GUI Curve Visualizer)

To render the magnitude response of a biquad filter in a UI, calculate the magnitude $|H(e^{j\omega})|$ at target frequencies $f$:

$$\omega = 2\pi \frac{f}{F_s}$$

$$\phi = 4 \sin^2\left(\frac{\omega}{2}\right)$$

$$|H(f)|^2 = \frac{(b_0+b_1+b_2)^2 + (b_0 b_2 \phi - b_1(b_0+b_2) - 4b_0 b_2)\phi}{(1+a_1+a_2)^2 + (a_2 \phi - a_1(1+a_2) - 4a_2)\phi}$$

$$\text{Magnitude}_{\text{dB}} = 10 \cdot \log_{10}\left(\max(|H(f)|^2, 10^{-12})\right)$$

---

## 5. Modern C++ Class Implementation

```cpp
#pragma once
#include <cmath>
#include <algorithm>

namespace VAudioDSP {

class Biquad {
public:
    enum class FilterType {
        Peaking,
        HighPass,
        LowPass,
        HighShelf,
        LowShelf
    };

    Biquad() = default;

    void setPeaking(float frequency, float Q, float gainDB, float sampleRate = 48000.0f) {
        float w0 = 2.0f * 3.14159265358979323846f * frequency / sampleRate;
        float alpha = std::sin(w0) / (2.0f * Q);
        float A = std::pow(10.0f, gainDB / 40.0f);
        float cos_w0 = std::cos(w0);

        float b0_r = 1.0f + alpha * A;
        float b1_r = -2.0f * cos_w0;
        float b2_r = 1.0f - alpha * A;
        float a0_r = 1.0f + alpha / A;
        float a1_r = -2.0f * cos_w0;
        float a2_r = 1.0f - alpha / A;

        b0 = b0_r / a0_r;
        b1 = b1_r / a0_r;
        b2 = b2_r / a0_r;
        a1 = a1_r / a0_r;
        a2 = a2_r / a0_r;
    }

    void setHighPass(float frequency, float Q, float sampleRate = 48000.0f) {
        float w0 = 2.0f * 3.14159265358979323846f * frequency / sampleRate;
        float alpha = std::sin(w0) / (2.0f * Q);
        float cos_w0 = std::cos(w0);

        float b0_r = (1.0f + cos_w0) / 2.0f;
        float b1_r = -(1.0f + cos_w0);
        float b2_r = (1.0f + cos_w0) / 2.0f;
        float a0_r = 1.0f + alpha;
        float a1_r = -2.0f * cos_w0;
        float a2_r = 1.0f - alpha;

        b0 = b0_r / a0_r;
        b1 = b1_r / a0_r;
        b2 = b2_r / a0_r;
        a1 = a1_r / a0_r;
        a2 = a2_r / a0_r;
    }

    // Direct Form II Transposed per-sample processing
    inline float process(float input) {
        float output = b0 * input + z1;
        z1 = b1 * input - a1 * output + z2;
        z2 = b2 * input - a2 * output;
        return output;
    }

    void reset() {
        z1 = 0.0f;
        z2 = 0.0f;
    }

private:
    float b0{1.0f}, b1{0.0f}, b2{0.0f};
    float a1{0.0f}, a2{0.0f};
    float z1{0.0f}, z2{0.0f};
};

} // namespace VAudioDSP
```
