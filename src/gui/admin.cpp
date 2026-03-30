// SPDX-License-Identifier: MIT
// Copyright (c) Scoria Developers Portal. See LICENSE.

#include "gui/admin.h"

#include <windows.h>
#include <shellapi.h>

#include <string>
#include <vector>

namespace gui {

static bool token_is_elevated() {
  HANDLE token = nullptr;
  if (!OpenProcessToken(GetCurrentProcess(), TOKEN_QUERY, &token)) return false;

  TOKEN_ELEVATION elev{};
  DWORD ret_len = 0;
  const BOOL ok = GetTokenInformation(token, TokenElevation, &elev, sizeof(elev), &ret_len);
  CloseHandle(token);
  return ok && elev.TokenIsElevated != 0;
}

bool is_running_as_admin() {
  return token_is_elevated();
}

static std::wstring quote_arg(const std::wstring& a) {
  if (a.empty()) return L"\"\"";
  if (a.find_first_of(L" \t\n\v\"") == std::wstring::npos) return a;

  std::wstring out = L"\"";
  for (wchar_t ch : a) {
    if (ch == L'"') out += L'\\';
    out += ch;
  }
  out += L"\"";
  return out;
}

bool relaunch_as_admin() {
  wchar_t exe_path[MAX_PATH]{};
  if (!GetModuleFileNameW(nullptr, exe_path, MAX_PATH)) return false;

  int argc = 0;
  LPWSTR* argv = CommandLineToArgvW(GetCommandLineW(), &argc);
  if (!argv) return false;

  // Rebuild arguments excluding argv[0]
  std::wstring params;
  for (int i = 1; i < argc; i++) {
    if (!params.empty()) params += L" ";
    params += quote_arg(argv[i]);
  }
  LocalFree(argv);

  HINSTANCE res = ShellExecuteW(nullptr, L"runas", exe_path, params.c_str(), nullptr, SW_SHOWNORMAL);
  return reinterpret_cast<INT_PTR>(res) > 32;
}

}  // namespace gui

