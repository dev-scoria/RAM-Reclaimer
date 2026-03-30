// SPDX-License-Identifier: MIT
// Copyright (c) Scoria Developers Portal. See LICENSE.

#include "app/console_win.h"

#include <windows.h>

#include <cstdio>
#include <cstdlib>
#include <cstring>
#include <iostream>

#ifndef ENABLE_VIRTUAL_TERMINAL_PROCESSING
#define ENABLE_VIRTUAL_TERMINAL_PROCESSING 0x0004
#endif

namespace ramc::console {
namespace {

bool enable_vt(HANDLE h) {
  if (h == INVALID_HANDLE_VALUE || h == nullptr) return false;
  DWORD mode = 0;
  if (!GetConsoleMode(h, &mode)) return false;
  return SetConsoleMode(h, mode | ENABLE_VIRTUAL_TERMINAL_PROCESSING) != 0;
}

bool windows_apps_use_dark_mode() {
  DWORD val = 1;
  DWORD sz = sizeof(val);
  HKEY key{};
  if (RegOpenKeyExW(HKEY_CURRENT_USER,
                    L"Software\\Microsoft\\Windows\\CurrentVersion\\Themes\\Personalize", 0, KEY_READ,
                    &key) != ERROR_SUCCESS) {
    return false;
  }
  const LSTATUS q =
      RegQueryValueExW(key, L"AppsUseLightTheme", nullptr, nullptr, reinterpret_cast<LPBYTE>(&val), &sz);
  RegCloseKey(key);
  if (q != ERROR_SUCCESS) return false;
  return val == 0;
}

bool env_force_dark_console() {
  wchar_t buf[8]{};
  return GetEnvironmentVariableW(L"RAMC_CONSOLE_DARK", buf,
                                   static_cast<DWORD>(sizeof(buf) / sizeof(buf[0]))) > 0;
}

void write_ansi(HANDLE h, const char* s) {
  if (h == INVALID_HANDLE_VALUE || h == nullptr || s == nullptr) return;
  const size_t n = strlen(s);
  if (n == 0) return;
  DWORD written = 0;
  WriteFile(h, s, static_cast<DWORD>(n), &written, nullptr);
}

void reset_vt() {
  const char reset[] = "\x1b[0m";
  write_ansi(GetStdHandle(STD_OUTPUT_HANDLE), reset);
  write_ansi(GetStdHandle(STD_ERROR_HANDLE), reset);
}

}  // namespace

void init() {
  HANDLE out = GetStdHandle(STD_OUTPUT_HANDLE);
  HANDLE err = GetStdHandle(STD_ERROR_HANDLE);

  const bool want_dark = windows_apps_use_dark_mode() || env_force_dark_console();
  if (!want_dark) return;

  const bool vt_out = enable_vt(out);
  if (!vt_out) return;

  if (err != out && err != INVALID_HANDLE_VALUE) {
    enable_vt(err);
  }

  // True-color SGR, then clear buffer so legacy conhost is not left white before the first line.
  constexpr const char dim[] =
      "\x1b[48;2;48;48;52m"
      "\x1b[38;2;222;222;224m"
      "\x1b[2J"
      "\x1b[H";
  write_ansi(out, dim);
  write_ansi(err, dim);

  fflush(stdout);
  fflush(stderr);
  std::cout.flush();
  std::cerr.flush();

  std::atexit(reset_vt);
}

}  // namespace ramc::console
