#pragma once
#include <algorithm>
#include <cmath>
#include <numbers>
#include <optional>

namespace flux {
struct ResizeRect {
    double x{}, y{}, width{}, height{}, rotation{};
};

// Pointer deltas are in slide coordinates, measured from the press position.
// An edge handle uses zero on its inactive axis; corners use +/-1 on both.
// Rotation is about the rectangle's center, as in Qt Quick. The opposite
// corner (or edge midpoint) stays fixed unless resizing about the center.
inline std::optional<ResizeRect> resizeRect(const ResizeRect& original, int horizontal,
                                           int vertical, double dx, double dy,
                                           bool aspectLock, bool fromCenter) {
    if (horizontal < -1 || horizontal > 1 || vertical < -1 || vertical > 1 ||
        (horizontal == 0 && vertical == 0) || !std::isfinite(dx) || !std::isfinite(dy) ||
        !std::isfinite(original.x) || !std::isfinite(original.y) ||
        !std::isfinite(original.width) || !std::isfinite(original.height) ||
        !std::isfinite(original.rotation) || std::abs(original.x) > 20000 ||
        std::abs(original.y) > 20000 || original.width < 1 || original.width > 20000 ||
        original.height < 1 || original.height > 20000)
        return std::nullopt;
    if (dx == 0 && dy == 0)
        return original;

    const double radians = std::remainder(original.rotation, 360.) * std::numbers::pi / 180.;
    const double c = std::cos(radians), s = std::sin(radians);
    const double localX = c * dx + s * dy, localY = -s * dx + c * dy;
    if (!std::isfinite(localX) || !std::isfinite(localY))
        return std::nullopt;
    const double multiplier = fromCenter ? 2 : 1;
    // Imported decks can contain dimensions outside the editor's usual limits.
    // Preserve those on a no-op and allow movement back toward the normal range.
    const double minW = std::min(16., original.width), maxW = std::max(10000., original.width);
    const double minH = std::min(16., original.height), maxH = std::max(10000., original.height);
    double width = original.width, height = original.height;
    if (aspectLock) {
        double factor;
        if (horizontal && vertical)
            factor = 1 + multiplier *
                             (localX * horizontal * width + localY * vertical * height) /
                             (width * width + height * height);
        else if (horizontal)
            factor = 1 + multiplier * localX * horizontal / width;
        else
            factor = 1 + multiplier * localY * vertical / height;
        if (!std::isfinite(factor))
            return std::nullopt;
        factor = std::clamp(factor, std::max(minW / width, minH / height),
                            std::min(maxW / width, maxH / height));
        width *= factor;
        height *= factor;
    } else {
        if (horizontal)
            width = std::clamp(width + multiplier * localX * horizontal, minW, maxW);
        if (vertical)
            height = std::clamp(height + multiplier * localY * vertical, minH, maxH);
    }
    const double dw = width - original.width, dh = height - original.height;
    const double centerX = fromCenter ? 0 : (c * horizontal * dw - s * vertical * dh) / 2;
    const double centerY = fromCenter ? 0 : (s * horizontal * dw + c * vertical * dh) / 2;
    const double moveX = centerX - dw / 2, moveY = centerY - dh / 2;
    // Constrain along the proposed geometry change, never clamp the position
    // independently: doing so would move the fixed anchor or break the ratio.
    double t = 1;
    auto constrainPosition = [&](double value, double delta) {
        if (delta > 0)
            t = std::min(t, (20000. - value) / delta);
        else if (delta < 0)
            t = std::min(t, (-20000. - value) / delta);
    };
    constrainPosition(original.x, moveX);
    constrainPosition(original.y, moveY);
    if (t < 1 && t > 0)
        t = std::nextafter(t, 0.); // Stay inside limits despite floating-point rounding.
    return ResizeRect{original.x + moveX * t, original.y + moveY * t,
                      original.width + dw * t, original.height + dh * t, original.rotation};
}
} // namespace flux
