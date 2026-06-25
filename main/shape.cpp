#include "shape.h"
#include <algorithm>
#include <cmath>

Shape generateShape(int numPoints,
                    double irregularity,
                    double noise,
                    double baseRadius,
                    std::mt19937& rng) {
    constexpr double TWO_PI = 6.28318530717958647692;
    const double step = TWO_PI / numPoints;

    // 1. Jittered angles around the circle.
    std::uniform_real_distribution<double> angleJitter(-irregularity * step,
                                                       +irregularity * step);
    std::vector<double> angles(numPoints);
    for (int i = 0; i < numPoints; ++i) {
        angles[i] = i * step + angleJitter(rng);
    }
    // Sort so we trace the polygon in order — prevents self-intersection.
    std::sort(angles.begin(), angles.end());

    // 2. Perturbed radius at each angle, polar -> cartesian.
    std::uniform_real_distribution<double> radiusJitter(-noise, +noise);
    Shape s;
    s.points.reserve(numPoints);
    for (double a : angles) {
        double r = baseRadius * (1.0 + radiusJitter(rng));
        s.points.push_back({ r * std::cos(a), r * std::sin(a) });
    }
    return s;
}
