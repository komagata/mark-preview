#pragma once

namespace mp {

enum class Theme { Light, Dark };

class ThemeDetector {
public:
    // Read system color scheme via wxSystemSettings.
    static Theme Detect();
};

inline const char* ThemeName(Theme t) { return t == Theme::Dark ? "dark" : "light"; }

} // namespace mp
