#pragma once

#include <cstdint>

namespace gui {

enum class ThemeMode : std::uint8_t {
  System = 0,
  Light = 1,
  Dark = 2,
};

enum class Language : std::uint8_t {
  English = 0,
  Russian = 1,
  German = 2,
};

struct Settings {
  ThemeMode theme = ThemeMode::System;
  Language language = Language::English;
  std::uint32_t refresh_seconds = 3;  // status auto-refresh
};

Settings load_settings();
void save_settings(const Settings& s);

}  // namespace gui

