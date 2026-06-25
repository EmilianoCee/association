#pragma once
#include <vector>
#include <random>

struct Point {
    double x, y;
};

struct Shape {
    std::vector<Point> points;  // vertices in order, connected tip-to-tail
};

// Generate one random shape using polar-coordinate perturbation.
//   numPoints     — vertex count (e.g. 3 to ~50)
//   irregularity  — 0.0 = perfectly even angles, 1.0 = wildly uneven
//   noise         — 0.0 = perfect ring, 1.0 = radii vary by ±100%
//   baseRadius    — nominal size in pixels
Shape generateShape(int numPoints,
                    double irregularity,
                    double noise,
                    double baseRadius,
                    std::mt19937& rng);
