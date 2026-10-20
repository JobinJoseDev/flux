#pragma once
#include <algorithm>
#include <cmath>
#include <numbers>

namespace flux {
struct Point {
    double x{}, y{};
};
struct Color {
    double r{}, g{}, b{}, a{1};
};
struct Pose {
    Point position{};
    double width{100}, height{100}, rotation{}, opacity{1};
    Color fill{}, ink{};
};
enum class Ease { Linear, Smooth, CubicOut };
enum class Path { Straight, Arc, Bezier };
inline double unit(double t) {
    return std::isfinite(t) ? std::clamp(t, 0.0, 1.0) : 0;
}
inline double mix(double a, double b, double t) {
    return a + (b - a) * t;
}
inline double ease(double t, Ease kind = Ease::Smooth) {
    t = unit(t);
    if (kind == Ease::Linear)
        return t;
    if (kind == Ease::CubicOut)
        return 1 - std::pow(1 - t, 3);
    return t * t * (3 - 2 * t);
}
inline Point cubic(Point a, Point c1, Point c2, Point b, double t) {
    t = unit(t);
    const double s = 1 - t;
    return {s * s * s * a.x + 3 * s * s * t * c1.x + 3 * s * t * t * c2.x + t * t * t * b.x,
            s * s * s * a.y + 3 * s * s * t * c1.y + 3 * s * t * t * c2.y + t * t * t * b.y};
}
inline Color blend(Color a, Color b, double t) {
    return {mix(a.r, b.r, t), mix(a.g, b.g, t), mix(a.b, b.b, t), mix(a.a, b.a, t)};
}
inline Pose interpolate(const Pose& a, const Pose& b, double t, Path path = Path::Straight,
                        Point c1 = {}, Point c2 = {}) {
    t = unit(t);
    Point p{mix(a.position.x, b.position.x, t), mix(a.position.y, b.position.y, t)};
    if (path == Path::Arc) {
        const double dx = b.position.x - a.position.x, dy = b.position.y - a.position.y;
        c1 = {a.position.x + dx / 3 - dy * 0.32, a.position.y + dy / 3 + dx * 0.32};
        c2 = {a.position.x + dx * 2 / 3 - dy * 0.32, a.position.y + dy * 2 / 3 + dx * 0.32};
    }
    if (path != Path::Straight)
        p = cubic(a.position, c1, c2, b.position, t);
    // Shortest rotation, avoiding a full spin at the 0/360-degree boundary.
    const double angle = std::remainder(b.rotation - a.rotation, 360.0);
    return {p,
            mix(a.width, b.width, t),
            mix(a.height, b.height, t),
            a.rotation + angle * t,
            mix(a.opacity, b.opacity, t),
            blend(a.fill, b.fill, t),
            blend(a.ink, b.ink, t)};
}
inline double localProgress(double time, double delay, double duration) {
    return duration <= 0 ? (time >= delay ? 1 : 0) : unit((time - delay) / duration);
}
} // namespace flux
