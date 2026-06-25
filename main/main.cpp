#include "shape.h"
#include <fstream>
#include <iostream>
#include <random>
#include <string>
#include <vector>

// One row in the sampler grid: parameters + a human label.
struct ShapeSpec {
    int numPoints;
    double irregularity;
    double noise;
    std::string label;
};

int main() {
    std::mt19937 rng(std::random_device{}());

    // A spread across the parameter space so you can see what each knob does.
    std::vector<ShapeSpec> specs = {
        { 8,  0.0, 0.00, "N=8  reg   smooth"  },
        { 8,  0.0, 0.40, "N=8  reg   noisy"   },
        { 8,  0.5, 0.00, "N=8  irreg smooth"  },
        { 38,  0.5, 0.40, "N=8  irreg noisy"   },
        { 24, 0.0, 0.15, "N=24 reg   mild"    },
        { 24, 0.6, 0.40, "N=24 wild"          },
    };

    // Lay shapes out on a 3x2 grid.
    const int cols = 3, rows = 2;
    const int cellSize = 220;
    const double baseRadius = 80.0;
    const int width  = cols * cellSize;
    const int height = rows * cellSize;

    std::ofstream out("shapes.svg");
    out << "<?xml version=\"1.0\" encoding=\"UTF-8\"?>\n";
    out << "<svg xmlns=\"http://www.w3.org/2000/svg\" width=\"" << width
        << "\" height=\"" << height
        << "\" viewBox=\"0 0 " << width << " " << height << "\">\n";
    out << "  <rect width=\"100%\" height=\"100%\" fill=\"#111\"/>\n";

    for (size_t i = 0; i < specs.size(); ++i) {
        const auto& spec = specs[i];
        Shape s = generateShape(spec.numPoints, spec.irregularity,
                                spec.noise, baseRadius, rng);

        // Find where this shape's cell sits on the canvas.
        int col = (int)(i % cols);
        int row = (int)(i / cols);
        double cx = col * cellSize + cellSize / 2.0;
        double cy = row * cellSize + cellSize / 2.0;

        // Translate math-coords (centered, y-up) into SVG coords (top-left, y-down).
        out << "  <polygon points=\"";
        for (const auto& p : s.points) {
            out << (cx + p.x) << "," << (cy - p.y) << " ";
        }
        out << "\" fill=\"#e8d8a0\" fill-opacity=\"0.15\" "
            << "stroke=\"#e8d8a0\" stroke-width=\"1\" />\n";

        // Label at the bottom of the cell.
        out << "  <text x=\"" << cx
            << "\" y=\"" << (row + 1) * cellSize - 10
            << "\" fill=\"#888\" font-family=\"monospace\" font-size=\"11\" "
            << "text-anchor=\"middle\">" << spec.label << "</text>\n";
    }

    out << "</svg>\n";
    std::cout << "Wrote shapes.svg — open it with:  open shapes.svg\n";
    return 0;
}
