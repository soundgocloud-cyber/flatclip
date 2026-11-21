// Standalone DSP sanity tests: g++ -std=c++17 -O2 -I../source dsp_test.cpp -o dsp_test && ./dsp_test
#include "FlatTopDSP.h"
#include <cstdio>
#include <vector>
#include <complex>
using namespace flattop;

static int fails = 0;
#define CHECK(cond, ...) do { if (!(cond)) { ++fails; std::printf("  FAIL: "); std::printf(__VA_ARGS__); std::printf("\n"); } else { std::printf("  ok:   "); std::printf(__VA_ARGS__); std::printf("\n"); } } while (0)

static double fs = 48000.0;

// amplitude of a sinusoid at f in x (single-bin DFT over the tail)
static double binAmp (const std::vector<float>& x, double f, size_t start)
{
    std::complex<double> acc = 0; size_t n = 0;
    for (size_t i = start; i < x.size(); ++i, ++n)
        acc += (double) x[i] * std::exp (std::complex<double> (0, -2.0 * kPi * f * (double) i / fs));
    return 2.0 * std::abs (acc) / (double) n;
}

static std::vector<float> run (const Params& p, const std::vector<float>& in)
{
    Engine e; e.prepare (fs); e.setParams (p, true);
    std::vector<float> L = in, R = in; float* d[2] = { L.data(), R.data() };
    const int block = 256;
    for (size_t i = 0; i < in.size(); i += block)
    {
        float* b[2] = { d[0] + i, d[1] + i };
        e.process (b, 2, (int) std::min<size_t> (block, in.size() - i));
    }
    return L;
}

static std::vector<float> sine (double f, double amp, size_t n)
{
    std::vector<float> v (n);
    for (size_t i = 0; i < n; ++i) v[i] = (float) (amp * std::sin (2.0 * kPi * f * (double) i / fs));
    return v;
}

static Params cleanParams()
{
    Params p; p.thresholdOn = false; p.sat = 0; p.sheen = 0; p.density = 0; return p;
}

int main()
{
    const size_t N = 96000;

    std::printf("[1] soft clip curve\n");
    {
        float prev = -2; bool mono = true; float mx = 0;
        for (float u = -20; u <= 20; u += 0.01f) { float y = softClip (u); mono &= y >= prev; prev = y; mx = std::max (mx, std::fabs (y)); }
        CHECK (mono, "monotonic");
        CHECK (mx <= 1.0f, "never exceeds 1.0 (max %.5f)", mx);
        CHECK (std::fabs (softClip (0.3f) - 0.3f) < 1e-6f, "linear below knee");
        CHECK (std::fabs (softClip (0.5001f) - softClip (0.4999f)) < 1e-3f, "continuous at knee");
    }

    std::printf("[2] makeup: threshold -12 dB lifts quiet material by +12 dB\n");
    {
        Params p = cleanParams(); p.thresholdOn = true; p.thresholdDb = -12.0f; p.sat = 0;
        auto y = run (p, sine (1000, 0.05, N));
        double a = binAmp (y, 1000, N / 2), expect = 0.05 * dbToGain (12.0f);
        CHECK (std::fabs (20 * std::log10 (a / expect)) < 0.3, "gain %.2f dB (expected 12.00)", 20 * std::log10 (a / 0.05));
    }

    std::printf("[3] threshold OFF is transparent (flat chain)\n");
    {
        Params p = cleanParams();
        auto y = run (p, sine (1000, 0.5, N));
        double a = binAmp (y, 1000, N / 2);
        CHECK (std::fabs (20 * std::log10 (a / 0.5)) < 0.05, "1 kHz gain %.3f dB", 20 * std::log10 (a / 0.5));
    }

    std::printf("[4] MULTI crossover sums flat (threshold off, drives 0 dB)\n");
    {
        Params p = cleanParams(); p.multi = true;
        for (double f : { 40.0, 80.0, 120.0, 300.0, 1000.0, 3000.0, 4000.0, 6000.0, 12000.0 })
        {
            auto y = run (p, sine (f, 0.4, N));
            double a = binAmp (y, f, N / 2);
            double err = 20 * std::log10 (a / 0.4);
            CHECK (std::fabs (err) < 0.35, "%7.0f Hz: %+.3f dB", f, err);
        }
    }

    std::printf("[5] MULTI: low drive affects 60 Hz but leaves 10 kHz alone\n");
    {
        Params p = cleanParams(); p.multi = true; p.driveLowDb = 12.0f;
        double a60 = binAmp (run (p, sine (60, 0.2, N)), 60, N / 2);
        double a10k = binAmp (run (p, sine (10000, 0.2, N)), 10000, N / 2);
        CHECK (20 * std::log10 (a60 / 0.2) > 10.0, "60 Hz: %+.2f dB", 20 * std::log10 (a60 / 0.2));
        CHECK (std::fabs (20 * std::log10 (a10k / 0.2)) < 0.5, "10 kHz: %+.2f dB", 20 * std::log10 (a10k / 0.2));
    }

    std::printf("[6] EQ: low shelf +12 dB, mid -12 dB, high shelf +12 dB\n");
    {
        Params p = cleanParams(); p.eqLowDb = 12; p.eqMidDb = -12; p.eqHighDb = 12;
        double lo = binAmp (run (p, sine (30, 0.1, N)), 30, N / 2) / 0.1;
        double mid = binAmp (run (p, sine (1000, 0.1, N)), 1000, N / 2) / 0.1;
        double hi = binAmp (run (p, sine (16000, 0.1, N)), 16000, N / 2) / 0.1;
        CHECK (20 * std::log10 (lo) > 10.5, "30 Hz %+.2f dB", 20 * std::log10 (lo));
        CHECK (20 * std::log10 (mid) < -10.0, "1 kHz %+.2f dB", 20 * std::log10 (mid));
        CHECK (20 * std::log10 (hi) > 10.5, "16 kHz %+.2f dB", 20 * std::log10 (hi));
    }

    std::printf("[7] EQ boost gets clipped by the ceiling\n");
    {
        Params p = cleanParams(); p.eqMidDb = 12; // +12 dB at 1k of a 0.8 sine -> 3.2, must be clipped at 1.0
        auto y = run (p, sine (1000, 0.8, N));
        float mx = 0; for (float v : y) mx = std::max (mx, std::fabs (v));
        CHECK (mx <= 1.0f && mx > 0.999f, "peak %.5f (flat-topped at 0 dBFS)", mx);
    }

    std::printf("[8] density pushes into ceiling: more flat-top samples, hard cap at 0 dBFS\n");
    {
        auto in = sine (200, 0.6, N);
        auto frac = [&] (float d) {
            Params p = cleanParams(); p.density = d; auto y = run (p, in);
            size_t c = 0, n = 0; float mx = 0;
            for (size_t i = N / 2; i < N; ++i, ++n) { if (std::fabs (y[i]) >= 0.9999f) ++c; mx = std::max (mx, std::fabs (y[i])); }
            return std::make_pair ((double) c / (double) n, mx);
        };
        auto a = frac (0.0f), b = frac (0.5f), c = frac (1.0f);
        CHECK (a.first < 0.001 && b.first > a.first && c.first > b.first, "flat-top %% : %.1f -> %.1f -> %.1f", a.first * 100, b.first * 100, c.first * 100);
        CHECK (c.second <= 1.0f, "max |y| = %.5f", c.second);
    }

    std::printf("[9] output never exceeds 0 dBFS under extreme settings\n");
    {
        Params p; p.inputDb = 24; p.thresholdDb = -24; p.postDb = 12; p.density = 1; p.sat = 1; p.sheen = 1; p.multi = true;
        p.driveLowDb = p.driveMidDb = p.driveHighDb = 24; p.eqLowDb = p.eqMidDb = p.eqHighDb = 12; p.outputDb = 6.0f;
        auto y = run (p, sine (500, 1.0, N));
        float mx = 0; bool finite = true; for (float v : y) { mx = std::max (mx, std::fabs (v)); finite &= std::isfinite (v); }
        CHECK (finite && mx <= 1.0f, "peak %.5f, finite=%d", mx, (int) finite);
    }

    std::printf("[10] sheen: 5 kHz in -> image appears at 5k + 3.9k = 8.9 kHz\n");
    {
        Params p = cleanParams(); p.sheen = 1.0f;
        auto y = run (p, sine (5000, 0.2, N));
        double img = binAmp (y, 8900, N / 2), fund = binAmp (y, 5000, N / 2), alias = binAmp (y, 1100 /* 5k-3.9k */, N / 2);
        std::printf("        fundamental %.3f, image@8.9k %.3f, @1.1k (lower sideband) %.4f\n", fund, img, alias);
        CHECK (img > 0.05, "image present (%.3f)", img);
        CHECK (alias < 0.1 * img, "single sideband: lower image %.1f dB below", 20 * std::log10 (img / std::max (alias, 1e-9)));
        auto y0 = run (cleanParams(), sine (5000, 0.2, N));
        CHECK (binAmp (y0, 8900, N / 2) < 0.002, "no image with sheen at 0");
    }

    std::printf("[11] sheen aliases (folds back) when shifted past Nyquist\n");
    {
        Params p = cleanParams(); p.sheen = 1.0f;
        // 21 kHz + 3.9 kHz = 24.9 kHz, above Nyquist (24 kHz @ 48k) -> folds down to 48 - 24.9 = 23.1 kHz
        auto y = run (p, sine (21000, 0.2, N));
        double fold = binAmp (y, 48000.0 - 24900.0, N / 2);
        CHECK (fold > 0.01, "21 kHz -> 24.9 kHz folds to 23.1 kHz: %.3f", fold);
    }

    std::printf("[12] stability across sample rates / silence\n");
    {
        for (double r : { 44100.0, 48000.0, 96000.0 })
        {
            fs = r; Params p; p.multi = true; p.sheen = 0.5f; p.sat = 0.5f;
            std::vector<float> z (20000, 0.0f);
            auto y = run (p, z);
            bool zero = true; for (float v : y) zero &= (std::fabs (v) < 1e-6f);
            CHECK (zero, "silence in -> silence out @ %.0f Hz", r);
        }
        fs = 48000.0;
    }

    std::printf("\n%s (%d failure%s)\n", fails ? "SOME TESTS FAILED" : "ALL TESTS PASSED", fails, fails == 1 ? "" : "s");
    return fails ? 1 : 0;
}
