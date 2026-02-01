// Simple Thin-Plate Spline RBF helper for small pin counts.
// Not tuned for very large systems; intended for interactive pin counts (<64).

#pragma once
#include <maya/MPoint.h>
#include <maya/MVector.h>
#include <vector>
#include <cmath>

namespace matchmesh {

struct PinSample {
    MPoint source;
    MVector delta; // target - source
};

inline double tpsKernel(double r) {
    const double eps = 1e-8;
    if (r < eps) return 0.0;
    double r2 = r * r;
    return r2 * std::log(r + eps);
}

// Dense, small linear solver using Gaussian elimination (pivoting omitted for brevity).
inline bool solveDense(std::vector<std::vector<double>>& A,
                       std::vector<double>& b,
                       std::vector<double>& x) {
    const size_t n = b.size();
    x.assign(n, 0.0);

    // Forward elimination with partial pivoting
    for (size_t i = 0; i < n; ++i) {
        size_t pivotRow = i;
        double pivotVal = std::abs(A[i][i]);
        for (size_t k = i + 1; k < n; ++k) {
            double v = std::abs(A[k][i]);
            if (v > pivotVal) { pivotVal = v; pivotRow = k; }
        }
        if (pivotVal < 1e-12) return false;
        if (pivotRow != i) {
            std::swap(A[i], A[pivotRow]);
            std::swap(b[i], b[pivotRow]);
        }
        double invPivot = 1.0 / A[i][i];
        for (size_t k = i + 1; k < n; ++k) {
            double factor = A[k][i] * invPivot;
            for (size_t j = i; j < n; ++j) A[k][j] -= factor * A[i][j];
            b[k] -= factor * b[i];
        }
    }

    // Back substitution
    for (int i = static_cast<int>(n) - 1; i >= 0; --i) {
        double sum = b[i];
        for (size_t j = i + 1; j < n; ++j) sum -= A[i][j] * x[j];
        x[i] = sum / A[i][i];
    }
    return true;
}

class RbfSolver {
public:
    void setPins(const std::vector<PinSample>& pins, double lambda = 0.0) {
        m_pins = pins;
        const size_t n = pins.size();
        m_weightsX.clear();
        m_weightsY.clear();
        m_weightsZ.clear();
        if (n == 0) return;

        std::vector<std::vector<double>> A(n, std::vector<double>(n, 0.0));
        for (size_t i = 0; i < n; ++i) {
            for (size_t j = 0; j < n; ++j) {
                double r = pins[i].source.distanceTo(pins[j].source);
                A[i][j] = tpsKernel(r);
            }
            A[i][i] += lambda;
        }

        std::vector<double> bx(n), by(n), bz(n);
        for (size_t i = 0; i < n; ++i) {
            bx[i] = pins[i].delta.x;
            by[i] = pins[i].delta.y;
            bz[i] = pins[i].delta.z;
        }
        solveDense(A, bx, m_weightsX);
        solveDense(A, by, m_weightsY);
        solveDense(A, bz, m_weightsZ);
    }

    MVector evaluate(const MPoint& p) const {
        const size_t n = m_pins.size();
        if (n == 0) return MVector::zero;
        double sx = 0.0, sy = 0.0, sz = 0.0;
        for (size_t i = 0; i < n; ++i) {
            double r = p.distanceTo(m_pins[i].source);
            double k = tpsKernel(r);
            sx += m_weightsX[i] * k;
            sy += m_weightsY[i] * k;
            sz += m_weightsZ[i] * k;
        }
        return MVector(sx, sy, sz);
    }

    size_t pinCount() const { return m_pins.size(); }

private:
    std::vector<PinSample> m_pins;
    std::vector<double> m_weightsX, m_weightsY, m_weightsZ;
};

} // namespace matchmesh
