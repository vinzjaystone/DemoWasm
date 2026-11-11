#pragma once

#include <type_traits>

namespace ashfall::raylib_compat {

// Raylib 5.5 takes integer x/y coordinates; 6.0 takes a Vector2 center.
// Inspect the selected header's function signature, not its version macros, so
// development builds/backports work too. Keep this independent of Raylib types
// so both call shapes can be tested without a graphics context or dependency.
template <typename DrawFunction, typename Center, typename Tint>
inline void drawCircleGradient(DrawFunction draw, Center center, float radius, Tint inner, Tint outer) {
    if constexpr (std::is_invocable_v<DrawFunction, Center, float, Tint, Tint>) {
        draw(center, radius, inner, outer);
    } else {
        static_assert(std::is_invocable_v<DrawFunction, int, int, float, Tint, Tint>,
                      "Unsupported Raylib DrawCircleGradient signature");
        // Preserve the original pixel-coordinate truncation for Raylib 5.5.
        draw(static_cast<int>(center.x), static_cast<int>(center.y), radius, inner, outer);
    }
}

} // namespace ashfall::raylib_compat
