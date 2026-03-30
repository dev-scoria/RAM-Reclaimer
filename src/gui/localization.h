// SPDX-License-Identifier: MIT
// Copyright (c) Scoria Developers Portal. See LICENSE.

#pragma once

#include "gui/settings.h"

#include <string>

namespace services {
struct MemoryStatus;
struct CleanResult;
}  // namespace services

namespace ramc::i18n {

enum class StringId : int {
  WindowTitle,
  HeaderProduct,
  HeaderVendor,
  FooterCredit,
  SectionStatus,
  SectionLog,
  BtnRefresh,
  BtnTrimSelf,
  BtnTrimAll,
  BtnSettings,
  BtnElevate,
  BtnAdminLocked,
  SettingsDlgTitle,
  SettingsPreferences,
  SettingsTheme,
  ThemeMatchSystem,
  ThemeLight,
  ThemeDark,
  SettingsRefreshInterval,
  SettingsOk,
  SettingsCancel,
  SettingsLanguage,
  MsgAppStarted,
  MsgSettingsSaved,
  MsgUacRequest,
  MsgUacFailed,
  MsgTrimStartSelf,
  MsgTrimStartAll,
  FmtPhysicalLine,
  FmtPagefileLine,
  FmtLoadLine,
  FmtTrimComplete,
  ErrRegisterClass,
  ErrCreateWindow,
  Count
};

void set_language(gui::Language lang);
const wchar_t* tr(StringId id);

std::wstring format_status(const services::MemoryStatus& st);
std::wstring msg_trim_complete(const services::CleanResult& r);

}  // namespace ramc::i18n
