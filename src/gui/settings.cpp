// SPDX-License-Identifier: MIT
// Copyright (c) Scoria Developers Portal. See LICENSE.

#include "gui/settings.h"

#include <windows.h>
#include <shlobj.h>

#include <algorithm>
#include <string>

namespace gui {

static std::wstring settings_path() {
  PWSTR dir = nullptr;
  if (FAILED(SHGetKnownFolderPath(FOLDERID_RoamingAppData, 0, nullptr, &dir)) || !dir) {
    return L"ramc_settings.ini";
  }

  std::wstring base(dir);
  CoTaskMemFree(dir);

  std::wstring folder = base + L"\\RAMC";
  CreateDirectoryW(folder.c_str(), nullptr);

  return folder + L"\\settings.ini";
}

static int theme_to_int(ThemeMode m) {
  switch (m) {
    case ThemeMode::System: return 0;
    case ThemeMode::Light: return 1;
    case ThemeMode::Dark: return 2;
  }
  return 0;
}

static ThemeMode theme_from_int(int v) {
  if (v == 1) return ThemeMode::Light;
  if (v == 2) return ThemeMode::Dark;
  return ThemeMode::System;
}

static int language_to_int(Language m) {
  return static_cast<int>(m);
}

static Language language_from_int(int v) {
  if (v == 1) return Language::Russian;
  if (v == 2) return Language::German;
  return Language::English;
}

Settings load_settings() {
  Settings s{};
  const auto path = settings_path();

  const UINT theme = GetPrivateProfileIntW(L"ui", L"theme", 0, path.c_str());
  s.theme = theme_from_int(static_cast<int>(theme));

  const UINT refresh = GetPrivateProfileIntW(L"ui", L"refresh_seconds", s.refresh_seconds, path.c_str());
  s.refresh_seconds = static_cast<std::uint32_t>(std::clamp<UINT>(refresh, 1, 3600));

  const UINT lang = GetPrivateProfileIntW(L"ui", L"language", 0, path.c_str());
  s.language = language_from_int(static_cast<int>(std::clamp<UINT>(lang, 0, 2)));

  return s;
}

void save_settings(const Settings& s) {
  const auto path = settings_path();

  wchar_t buf[32]{};

  _snwprintf_s(buf, _TRUNCATE, L"%d", theme_to_int(s.theme));
  WritePrivateProfileStringW(L"ui", L"theme", buf, path.c_str());

  _snwprintf_s(buf, _TRUNCATE, L"%u", static_cast<unsigned>(std::clamp<std::uint32_t>(s.refresh_seconds, 1, 3600)));
  WritePrivateProfileStringW(L"ui", L"refresh_seconds", buf, path.c_str());

  _snwprintf_s(buf, _TRUNCATE, L"%d", language_to_int(s.language));
  WritePrivateProfileStringW(L"ui", L"language", buf, path.c_str());
}

}  // namespace gui

