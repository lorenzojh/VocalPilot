#pragma once
#include "Transformation.h"
#include <complex>
namespace vocalpilot {
// Original iterative radix-2 implementation; twiddles and permutation prepared
// once. Inverse includes 1/N. No allocations or transcendental calls in run().
class Radix2FFT {
public:
    using Complex = std::complex<double>;
    void prepare(int n) {
        size = n; reverse.resize(n); roots.resize(n / 2);
        for (int i = 0; i < n; ++i) {
            int a = i, b = 0;
            for (int k = n; k > 1; k >>= 1) { b = (b << 1) | (a & 1); a >>= 1; }
            reverse[i] = b;
        }
        for (int i = 0; i < n / 2; ++i) roots[i] = std::polar(1.0, -2 * transformPi * i / n);
    }
    void run(std::vector<Complex>& x, bool inverse = false) const noexcept {
        for (int i = 0; i < size; ++i) if (i < reverse[i]) std::swap(x[i], x[reverse[i]]);
        for (int width = 2; width <= size; width *= 2)
            for (int begin = 0; begin < size; begin += width)
                for (int j = 0; j < width / 2; ++j) {
                    const auto root = roots[j * (size / width)];
                    const auto t = x[begin + j + width / 2] * (inverse ? std::conj(root) : root);
                    const auto u = x[begin + j];
                    x[begin + j] = u + t; x[begin + j + width / 2] = u - t;
                }
        if (inverse) for (auto& v : x) v /= size;
    }
private:
    int size = 0;
    std::vector<int> reverse;
    std::vector<Complex> roots;
};
}
