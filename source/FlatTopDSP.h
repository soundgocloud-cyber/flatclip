// FlatTop DSP core - plain C++17, no JUCE dependency so it can be unit-tested on its own.
//
// Signal flow (per channel):
//   Input -> [soft clip w/ built-in makeup, SINGLE or MULTI 3-band] -> Post
//         -> 3-band EQ -> Sat -> Sheen (+3.9 kHz image) -> Density (drive)
//         -> HARD CLIP @ 0 dBFS (NOT oversampled, on purpose) -> Output
#pragma once
#include <algorithm>
#include <array>
#include <cmath>
#include <cstring>

namespace flattop
{
constexpr double kPi = 3.14159265358979323846;
constexpr float kSheenShiftHz = 3900.0f;

inline float dbToGain (float db) { return std::pow (10.0f, db * 0.05f); }

struct Params
{
    float inputDb = 0.0f;       // pre gain
    float thresholdDb = -6.0f;  // soft clip threshold (makeup = 1/threshold is built in)
    bool  thresholdOn = true;   // false = soft clip stage bypassed completely
    float postDb = 0.0f;        // level after the soft clip stage
    float outputDb = 0.0f;      // final level, after the hard ceiling (<= 0 dB)

    float sat = 0.2f;           // 0..1
    float sheen = 0.0f;         // 0..1
    float density = 0.0f;       // 0..1  -> 0..+12 dB drive into the ceiling

    bool  multi = false;
    float driveLowDb = 0.0f, driveMidDb = 0.0f, driveHighDb = 0.0f;
    float xoLowHz = 120.0f, xoHighHz = 4000.0f;

    float eqLowDb = 0.0f, eqMidDb = 0.0f, eqHighDb = 0.0f;
    float eqLowHz = 100.0f, eqMidHz = 1000.0f, eqHighHz = 8000.0f;
};

// ---------------------------------------------------------------- biquad (RBJ)
struct Biquad
{
    float b0 = 1, b1 = 0, b2 = 0, a1 = 0, a2 = 0;
    float z1 = 0, z2 = 0;

    inline float process (float x) noexcept
    {
        const float y = b0 * x + z1;
        z1 = b1 * x - a1 * y + z2;
        z2 = b2 * x - a2 * y;
        return y;
    }
    void reset() noexcept { z1 = z2 = 0; }

    void set (double nb0, double nb1, double nb2, double na0, double na1, double na2) noexcept
    {
        b0 = (float) (nb0 / na0); b1 = (float) (nb1 / na0); b2 = (float) (nb2 / na0);
        a1 = (float) (na1 / na0); a2 = (float) (na2 / na0);
    }

    static double clampFreq (double f, double fs) { return std::min (std::max (f, 10.0), fs * 0.45); }

    void lowPass (double fs, double f, double q)
    {
        const double w = 2.0 * kPi * clampFreq (f, fs) / fs, c = std::cos (w), al = std::sin (w) / (2.0 * q);
        set ((1 - c) * 0.5, 1 - c, (1 - c) * 0.5, 1 + al, -2 * c, 1 - al);
    }
    void highPass (double fs, double f, double q)
    {
        const double w = 2.0 * kPi * clampFreq (f, fs) / fs, c = std::cos (w), al = std::sin (w) / (2.0 * q);
        set ((1 + c) * 0.5, -(1 + c), (1 + c) * 0.5, 1 + al, -2 * c, 1 - al);
    }
    void peak (double fs, double f, double q, double gainDb)
    {
        const double A = std::pow (10.0, gainDb / 40.0);
        const double w = 2.0 * kPi * clampFreq (f, fs) / fs, c = std::cos (w), al = std::sin (w) / (2.0 * q);
        set (1 + al * A, -2 * c, 1 - al * A, 1 + al / A, -2 * c, 1 - al / A);
    }
    void lowShelf (double fs, double f, double q, double gainDb)
    {
        const double A = std::pow (10.0, gainDb / 40.0);
        const double w = 2.0 * kPi * clampFreq (f, fs) / fs, c = std::cos (w), al = std::sin (w) / (2.0 * q);
        const double t = 2.0 * std::sqrt (A) * al;
        set (A * ((A + 1) - (A - 1) * c + t), 2 * A * ((A - 1) - (A + 1) * c), A * ((A + 1) - (A - 1) * c - t),
             (A + 1) + (A - 1) * c + t, -2 * ((A - 1) + (A + 1) * c), (A + 1) + (A - 1) * c - t);
    }
    void highShelf (double fs, double f, double q, double gainDb)
    {
        const double A = std::pow (10.0, gainDb / 40.0);
        const double w = 2.0 * kPi * clampFreq (f, fs) / fs, c = std::cos (w), al = std::sin (w) / (2.0 * q);
        const double t = 2.0 * std::sqrt (A) * al;
        set (A * ((A + 1) + (A - 1) * c + t), -2 * A * ((A - 1) + (A + 1) * c), A * ((A + 1) + (A - 1) * c - t),
             (A + 1) - (A - 1) * c + t, 2 * ((A - 1) - (A + 1) * c), (A + 1) - (A - 1) * c - t);
    }
};

// 4th-order Linkwitz-Riley crossover (two cascaded Butterworth biquads per side)
struct LR4
{
    std::array<Biquad, 2> lp, hp;
    void setFreq (double fs, double f)
    {
        for (auto& b : lp) b.lowPass (fs, f, 0.70710678);
        for (auto& b : hp) b.highPass (fs, f, 0.70710678);
    }
    void reset() { for (auto& b : lp) b.reset(); for (auto& b : hp) b.reset(); }
    inline void split (float x, float& low, float& high) noexcept
    {
        low  = lp[1].process (lp[0].process (x));
        high = hp[1].process (hp[0].process (x));
    }
    // LP + HP of an LR4 is an all-pass; used to phase-match the low band to the 2nd crossover
    inline float allpass (float x) noexcept
    {
        float l, h; split (x, l, h); return l + h;
    }
};

// --------------------------------------------------- smoother (one pole)
struct Smoother
{
    float cur = 0, target = 0, coeff = 0.01f;
    void setTime (double fs, double ms) { coeff = 1.0f - (float) std::exp (-1.0 / (fs * ms * 0.001)); }
    void snap (float v) { cur = target = v; }
    inline float next() noexcept { cur += coeff * (target - cur); return cur; }
};

// --------------------------------------------------- Hilbert up-shifter
// Shifts content UP by kSheenShiftHz (single sideband). Intentionally NOT band-limited
// after the shift, so energy shifted past Nyquist folds back: that is the aliasing "image".
struct HilbertShifter
{
    static constexpr int N = 63, C = 31, MASK = 63;
    std::array<float, N> taps {};
    std::array<float, 64> buf {};
    int pos = 0;

    HilbertShifter()
    {
        for (int k = 0; k < N; ++k)
        {
            const int n = k - C;
            if ((n & 1) == 0) { taps[(size_t) k] = 0; continue; } // only odd offsets are non-zero
            const double w = 0.42 - 0.5 * std::cos (2.0 * kPi * k / (N - 1)) + 0.08 * std::cos (4.0 * kPi * k / (N - 1));
            taps[(size_t) k] = (float) (2.0 / (kPi * n) * w);
        }
    }
    void reset() { buf.fill (0.0f); pos = 0; }

    inline float process (float x, float cosv, float sinv) noexcept
    {
        buf[(size_t) pos] = x;
        float h = 0.0f;
        for (int k = 0; k < N; k += 2) // even k <-> odd offset
            h += taps[(size_t) k] * buf[(size_t) ((pos - k) & MASK)];
        const float xd = buf[(size_t) ((pos - C) & MASK)];
        pos = (pos + 1) & MASK;
        return xd * cosv - h * sinv; // upper sideband
    }
};

// ----------------------------------------------------------- the engine
inline float softClip (float u) noexcept
{
    // linear up to the knee, then a smooth tanh tail that settles at exactly 1.0
    constexpr float knee = 0.5f;
    const float a = std::fabs (u);
    if (a <= knee) return u;
    const float y = knee + (1.0f - knee) * std::tanh ((a - knee) / (1.0f - knee));
    return std::copysign (y, u);
}

class Engine
{
public:
    static constexpr int maxChannels = 2;

    void prepare (double sampleRate)
    {
        fs = sampleRate;
        lastF1 = lastF2 = -1.0; eqInit = false; hpInit = false;
        for (auto* s : { &gIn, &gPost, &gOut, &invT, &dLow, &dMid, &dHigh, &satAmt, &sheenAmt, &gDens, &threshMix, &multiMix })
            s->setTime (fs, 12.0);
        setParams (p, true);
        reset();
    }

    void reset()
    {
        for (auto& c : ch)
        {
            c.xo1.reset(); c.xo2.reset(); c.apLow.reset();
            for (auto& b : c.eq) b.reset();
            c.sheenHp.reset(); c.hilbert.reset();
        }
        phase = 0.0;
    }

    void setParams (const Params& np, bool snap = false)
    {
        p = np;
        const float thLin = std::max (dbToGain (std::min (np.thresholdDb, 0.0f)), 1.0e-3f);
        gIn.target = dbToGain (np.inputDb);
        gPost.target = dbToGain (np.postDb);
        gOut.target = dbToGain (std::min (np.outputDb, 0.0f)); // never above the 0 dBFS ceiling
        invT.target = 1.0f / thLin;                              // built-in makeup
        dLow.target = dbToGain (np.driveLowDb);
        dMid.target = dbToGain (np.driveMidDb);
        dHigh.target = dbToGain (np.driveHighDb);
        satAmt.target = std::clamp (np.sat, 0.0f, 1.0f);
        sheenAmt.target = std::clamp (np.sheen, 0.0f, 1.0f);
        gDens.target = dbToGain (std::clamp (np.density, 0.0f, 1.0f) * 12.0f);
        threshMix.target = np.thresholdOn ? 1.0f : 0.0f;
        multiMix.target = np.multi ? 1.0f : 0.0f;
        if (snap)
            for (auto* s : { &gIn, &gPost, &gOut, &invT, &dLow, &dMid, &dHigh, &satAmt, &sheenAmt, &gDens, &threshMix, &multiMix })
                s->snap (s->target);

        if (fs <= 0.0) return;
        const double f1 = std::clamp ((double) np.xoLowHz, 30.0, 1000.0);
        const double f2 = std::clamp ((double) np.xoHighHz, f1 * 1.5, 16000.0);
        if (f1 != lastF1 || f2 != lastF2)
        {
            lastF1 = f1; lastF2 = f2;
            for (auto& c : ch) { c.xo1.setFreq (fs, f1); c.xo2.setFreq (fs, f2); c.apLow.setFreq (fs, f2); }
        }
        if (np.eqLowDb != lastEq[0] || np.eqMidDb != lastEq[1] || np.eqHighDb != lastEq[2]
            || np.eqLowHz != lastEq[3] || np.eqMidHz != lastEq[4] || np.eqHighHz != lastEq[5] || ! eqInit)
        {
            eqInit = true;
            lastEq = { np.eqLowDb, np.eqMidDb, np.eqHighDb, np.eqLowHz, np.eqMidHz, np.eqHighHz };
            for (auto& c : ch)
            {
                c.eq[0].lowShelf (fs, np.eqLowHz, 0.7071, np.eqLowDb);
                c.eq[1].peak (fs, np.eqMidHz, 0.8, np.eqMidDb);
                c.eq[2].highShelf (fs, np.eqHighHz, 0.7071, np.eqHighDb);
            }
        }
        if (! hpInit) { hpInit = true; for (auto& c : ch) c.sheenHp.highPass (fs, 3000.0, 0.7071); }
    }

    // Process in place. data[c][i]; numCh <= 2.
    void process (float* const* data, int numCh, int numSamples)
    {
        numCh = std::min (numCh, maxChannels);
        const double inc = 2.0 * kPi * (double) kSheenShiftHz / fs;

        for (int i = 0; i < numSamples; ++i)
        {
            const float g_in = gIn.next(), g_post = gPost.next(), g_out = gOut.next(), it = invT.next();
            const float d_lo = dLow.next(), d_mi = dMid.next(), d_hi = dHigh.next();
            const float sat = satAmt.next(), sheen = sheenAmt.next(), g_dens = gDens.next();
            const float tm = threshMix.next(), mm = multiMix.next();

            const float cs = (float) std::cos (phase), sn = (float) std::sin (phase);
            phase += inc; if (phase > 2.0 * kPi) phase -= 2.0 * kPi;

            const float satGain = 1.0f + 3.0f * sat;
            const float satNorm = 1.0f / std::tanh (satGain);

            for (int c = 0; c < numCh; ++c)
            {
                auto& st = ch[(size_t) c];
                float x = data[c][i] * g_in;

                // ---- soft clip stage (makeup 1/T built in) ----
                float s = x;
                if (tm > 0.0f) s = tm * softClip (x * it) + (1.0f - tm) * x;

                float m = 0.0f;
                {
                    float low1, high1, mid, high;
                    st.xo1.split (x, low1, high1);
                    st.xo2.split (high1, mid, high);
                    const float low = st.apLow.allpass (low1);
                    auto band = [&] (float v, float d) {
                        v *= d;
                        return tm > 0.0f ? tm * softClip (v * it) + (1.0f - tm) * v : v;
                    };
                    m = band (low, d_lo) + band (mid, d_mi) + band (high, d_hi);
                }
                float y = (mm > 0.0f) ? (mm * m + (1.0f - mm) * s) : s;

                y *= g_post;

                // ---- 3-band master EQ (boosts land BEFORE sat + ceiling) ----
                y = st.eq[0].process (y);
                y = st.eq[1].process (y);
                y = st.eq[2].process (y);

                // ---- sat ----
                if (sat > 1.0e-4f)
                    y += sat * (std::tanh (satGain * y) * satNorm - y);

                // ---- sheen: +3.9 kHz image of the highs, left un-bandlimited (aliases) ----
                if (sheen > 1.0e-4f)
                    y += 0.6f * sheen * st.hilbert.process (st.sheenHp.process (y), cs, sn);
                else
                    st.hilbert.process (st.sheenHp.process (y), cs, sn); // keep state warm

                // ---- density: drive into the ceiling ----
                y *= g_dens;

                // ---- ceiling: hard clip at 0 dBFS, not oversampled ----
                if (! std::isfinite (y)) y = 0.0f;
                y = std::min (1.0f, std::max (-1.0f, y));

                data[c][i] = y * g_out;
            }
        }
    }

private:
    struct Chan
    {
        LR4 xo1, xo2, apLow;
        std::array<Biquad, 3> eq;
        Biquad sheenHp;
        HilbertShifter hilbert;
        Chan() {}
    };

    double fs = 0.0;
    Params p;
    std::array<Chan, maxChannels> ch;
    Smoother gIn, gPost, gOut, invT, dLow, dMid, dHigh, satAmt, sheenAmt, gDens, threshMix, multiMix;
    double phase = 0.0, lastF1 = -1.0, lastF2 = -1.0;
    std::array<float, 6> lastEq {};
    bool eqInit = false, hpInit = false;
};
} // namespace flattop
