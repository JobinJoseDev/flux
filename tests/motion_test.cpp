#include "core/motion.hpp"
#include <cstdlib>
#include <iostream>
#include <limits>
int count = 0;
void check(bool ok, const char* name) {
    ++count;
    if (!ok) {
        std::cerr << "FAIL: " << name << '\n';
        std::exit(1);
    }
}
bool near(double a, double b) {
    return std::abs(a - b) < 1e-8;
}
int main() {
    using namespace flux;
    Pose a{{0, 100}, 120, 90, 350, 0, {1, 0, 0, 1}, {1, 1, 1, 1}};
    Pose b{{300, 400}, 240, 180, 10, 1, {0, 0, 1, 1}, {0, 0, 0, 1}};
    auto p = interpolate(a, b, .5);
    check(near(p.position.x, 150) && near(p.position.y, 250), "matched object midpoint");
    check(near(p.width, 180) && near(p.height, 135), "resizing");
    check(near(p.rotation, 360), "shortest rotation across zero");
    check(near(p.opacity, .5), "appearing / disappearing object");
    check(near(p.fill.r, .5) && near(p.fill.b, .5), "color interpolation");
    for (auto path : {Path::Straight, Path::Arc, Path::Bezier}) {
        auto start = interpolate(a, b, 0, path, {200, -100}, {400, -100});
        auto end = interpolate(a, b, 1, path, {200, -100}, {400, -100});
        check(near(start.position.x, 0) && near(start.position.y, 100), "path exact start");
        check(near(end.position.x, 300) && near(end.position.y, 400), "path exact end");
    }
    check(cubic({0, 0}, {0, 100}, {100, 100}, {100, 0}, .5).y == 75, "cubic control points");
    check(near(ease(.5), .5) && ease(0) == 0 && ease(1) == 1, "smooth easing endpoints");
    check(unit(-1) == 0 && unit(2) == 1 && unit(std::numeric_limits<double>::quiet_NaN()) == 0,
          "progress guards");
    check(localProgress(.2, .4, .8) == 0 && localProgress(1.5, .4, .8) == 1, "delay clamps");
    check(near(localProgress(.8, .4, .8), .5), "per-object timeline");
    check(localProgress(.4, .4, 0) == 1, "zero-duration instantaneous build");
    double prev = -1;
    for (int i = 0; i <= 1000; ++i) {
        double e = ease(i / 1000.);
        check(e >= prev, "monotonic easing");
        prev = e;
    }
    std::cout << count << " motion checks passed\n";
}
