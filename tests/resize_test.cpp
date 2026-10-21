#include "core/resize.hpp"
#include <cstdlib>
#include <iostream>
#include <limits>
#include <utility>

namespace {
int checks = 0;
void check(bool value, const char* message) {
    ++checks;
    if (!value) {
        std::cerr << "FAIL: " << message << '\n';
        std::exit(1);
    }
}
bool near(double a, double b) { return std::abs(a - b) < 1e-8; }
std::pair<double, double> rotate(double x, double y, double degrees) {
    const double r = degrees * std::numbers::pi / 180.;
    return {std::cos(r) * x - std::sin(r) * y, std::sin(r) * x + std::cos(r) * y};
}
std::pair<double, double> anchor(const flux::ResizeRect& rect, int hx, int hy, bool center) {
    const auto [dx, dy] = rotate(center ? 0 : -hx * rect.width / 2,
                                 center ? 0 : -hy * rect.height / 2, rect.rotation);
    return {rect.x + rect.width / 2 + dx, rect.y + rect.height / 2 + dy};
}
void fixedAnchor(const flux::ResizeRect& a, const flux::ResizeRect& b, int hx, int hy,
                 bool center) {
    const auto [ax, ay] = anchor(a, hx, hy, center);
    const auto [bx, by] = anchor(b, hx, hy, center);
    check(near(ax, bx) && near(ay, by), "opposite anchor / center stays fixed in slide space");
}
} // namespace

int main() {
    using flux::resizeRect;
    using flux::ResizeRect;
    for (const double angle : {0., 30., 90., 135., 180., 270., -40., 350., 720.})
        for (int hx = -1; hx <= 1; ++hx)
            for (int hy = -1; hy <= 1; ++hy) {
                if (!hx && !hy)
                    continue;
                const ResizeRect original{300, 200, 200, 120, angle};
                const auto [dx, dy] = rotate(hx ? hx * 40. : 37., hy ? hy * 30. : 47., angle);
                for (const bool center : {false, true})
                    for (const bool lock : {false, true}) {
                        const auto result = resizeRect(original, hx, hy, dx, dy, lock, center);
                        check(result.has_value(), "all eight handles accept rotated slide deltas");
                        fixedAnchor(original, *result, hx, hy, center);
                        check(result->rotation == angle, "resize preserves rotation");
                        if (lock) {
                            check(near(result->width / result->height, 200. / 120.),
                                  "Shift preserves aspect ratio on every handle");
                            if (!hy)
                                check(near(result->width, 200 + (center ? 80 : 40)),
                                      "horizontal Shift edge follows active pointer axis");
                            if (!hx)
                                check(near(result->height, 120 + (center ? 60 : 30)),
                                      "vertical Shift edge follows active pointer axis");
                        } else {
                            check(near(result->width, 200 + (hx ? (center ? 80 : 40) : 0)),
                                  "unlocked horizontal size and inactive edge axis");
                            check(near(result->height, 120 + (hy ? (center ? 60 : 30) : 0)),
                                  "unlocked vertical size and inactive edge axis");
                        }
                        // Crossing the opposite edge must never mirror the object.
                        const auto [shrinkX, shrinkY] = rotate(-hx * 10000., -hy * 10000., angle);
                        const auto small = resizeRect(original, hx, hy, shrinkX, shrinkY, lock, center);
                        check(small && small->width >= 16 - 1e-8 && small->height >= 16 - 1e-8,
                              "crossing an anchor stops at the minimum positive extent");
                        fixedAnchor(original, *small, hx, hy, center);
                        if (lock)
                            check(near(small->width / small->height, 200. / 120.),
                                  "minimum extent does not break locked ratio");
                        const auto large = resizeRect(original, hx, hy, -shrinkX, -shrinkY, lock,
                                                       center);
                        check(large && large->width <= 10000 + 1e-8 &&
                                  large->height <= 10000 + 1e-8,
                              "maximum dimensions constrain resizing");
                        fixedAnchor(original, *large, hx, hy, center);
                        if (lock)
                            check(near(large->width / large->height, 200. / 120.),
                                  "maximum extent does not break locked ratio");
                    }
            }
    const ResizeRect nearBoundary{19990, 50, 200, 100, 180};
    const auto bounded = resizeRect(nearBoundary, 1, 1, 100, 50, true, false);
    check(bounded && near(bounded->x, 20000), "position limits stop geometry as one operation");
    check(near(bounded->width / bounded->height, 2), "position limit preserves locked ratio");
    fixedAnchor(nearBoundary, *bounded, 1, 1, false);
    const auto stopped = resizeRect(*bounded, 1, 1, 100, 50, true, false);
    check(stopped && near(stopped->width, bounded->width), "cannot drag beyond position limit");
    const auto inward = resizeRect(*bounded, 1, 1, -100, -50, true, false);
    check(inward && inward->x < bounded->x, "can drag back inward from the position limit");

    const ResizeRect original{300, 200, 200, 120, 30};
    const double nan = std::numeric_limits<double>::quiet_NaN();
    const double inf = std::numeric_limits<double>::infinity();
    check(!resizeRect(original, 0, 0, 10, 10, false, false), "reject absent handle");
    check(!resizeRect(original, 2, 1, 10, 10, false, false), "reject invalid handle");
    check(!resizeRect(original, 1, 1, nan, 10, false, false), "reject NaN pointer delta");
    check(!resizeRect(original, 1, 1, 10, inf, false, false), "reject infinite pointer delta");
    check(!resizeRect({0, 0, 0, 120, 0}, 1, 1, 10, 10, false, false), "reject zero dimension");
    check(!resizeRect({0, 0, 200, 120, inf}, 1, 1, 10, 10, false, false), "reject invalid angle");
    const auto zero = resizeRect(original, -1, -1, 0, 0, true, true);
    check(zero && zero->x == original.x && zero->y == original.y &&
              zero->width == original.width && zero->height == original.height,
          "zero pointer delta is an exact no-op");
    const ResizeRect imported{0, 0, 4, 16000, 0};
    const auto importedZero = resizeRect(imported, 1, 1, 0, 0, false, false);
    check(importedZero && importedZero->width == 4 && importedZero->height == 16000,
          "existing valid dimensions outside editing limits survive a no-op");
    const auto importedResize = resizeRect(imported, 1, 1, 10, -100, false, false);
    check(importedResize && importedResize->width == 14 && importedResize->height == 15900,
          "imported dimensions can move toward normal editing limits without jumping");
    std::cout << checks << " resize checks passed\n";
}
