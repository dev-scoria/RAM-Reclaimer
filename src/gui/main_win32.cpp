// SPDX-License-Identifier: MIT
// Copyright (c) Scoria Developers Portal. See LICENSE.

#include "services/cleaner.h"

#include "gui/admin.h"
#include "gui/settings.h"
#include "gui/localization.h"
#include "core/win/process_enum.h"
#include "core/win/working_set_trim.h"

#include <windows.h>
#include <commctrl.h>

#include <algorithm>
#include <string>
#include <thread>
#include <atomic>

namespace {

constexpr wchar_t kAppClass[] = L"RAMC_MAIN_WND";
constexpr wchar_t kHeaderClass[] = L"RAMC_HDR";
constexpr wchar_t kSettingsDlgClass[] = L"RAMC_SETTINGS_DLG";

constexpr int kHeaderHeight = 60;
constexpr int kContentPad = 12;
constexpr int kBtnH = 28;
constexpr int kBtnW = 116;
constexpr int kBtnGap = 6;
constexpr int kSectionLabelH = 16;
constexpr int kStatusBlockH = 86;
constexpr int kProgressH = 18;
constexpr int kBlockGap = 10;
constexpr int kCreditH = 28;
constexpr int kGapAfterLog = 14;
constexpr int kFooterAboveProgress = 14;

enum : int {
  IDC_BTN_STATUS = 101,
  IDC_BTN_CLEAN_SELF = 102,
  IDC_BTN_CLEAN_ALL = 103,
  IDC_BTN_SETTINGS = 104,
  IDC_BTN_ADMIN = 105,

  IDC_LBL_SECTION_STATUS = 106,
  IDC_LBL_SECTION_LOG = 107,
  IDC_LBL_CREDIT = 108,

  IDC_EDIT_STATUS = 201,
  IDC_EDIT_LOG = 202,
  IDC_PROGRESS = 203,
};

constexpr UINT_PTR IDT_AUTOREFRESH = 1;
constexpr UINT WM_APP_PROGRESS = WM_APP + 1;
constexpr UINT WM_APP_DONE = WM_APP + 2;

struct ThemePalette {
  COLORREF header_bar{};
  COLORREF header_title{};
  COLORREF header_subtitle{};
  COLORREF body{};
  COLORREF panel{};
  COLORREF panel_secondary{};
  COLORREF border{};
  COLORREF text_primary{};
  COLORREF text_secondary{};
  COLORREF text_muted{};
  COLORREF footer_text{};
  COLORREF footer_bar{};
  COLORREF progress_fg{};
  COLORREF progress_bg{};
};

struct ThemeBrushes {
  HBRUSH body = nullptr;
  HBRUSH panel = nullptr;
  HBRUSH panel_secondary = nullptr;
  HBRUSH footer = nullptr;
};

struct WorkerResult {
  services::CleanResult clean{};
  services::MemoryStatus before{};
  services::MemoryStatus after{};
};

struct AppState {
  gui::Settings settings{};
  ThemePalette pal{};
  ThemeBrushes brushes{};
  HFONT font_ui = nullptr;
  HFONT font_ui_semibold = nullptr;
  HFONT font_header_title = nullptr;
  HFONT font_header_sub = nullptr;
  HWND hwnd_header = nullptr;
  bool is_admin = false;
  std::atomic<bool> worker_running{false};
  std::atomic<std::uint32_t> worker_total{0};
  std::atomic<std::uint32_t> worker_done{0};
  int footer_strip_top = -1;
} g;

static COLORREF gray(std::uint8_t v) { return RGB(v, v, v); }

static ThemePalette palette_light() {
  return ThemePalette{
      .header_bar = gray(34),
      .header_title = gray(210),
      .header_subtitle = gray(152),
      .body = gray(244),
      .panel = gray(255),
      .panel_secondary = gray(251),
      .border = gray(198),
      .text_primary = gray(26),
      .text_secondary = gray(68),
      .text_muted = gray(112),
      .footer_text = gray(88),
      .footer_bar = gray(236),
      .progress_fg = gray(42),
      .progress_bg = gray(218),
  };
}

static ThemePalette palette_dark() {
  // Softer dark (not near-black): easier on the eyes, closer to VS / modern app dark UIs.
  return ThemePalette{
      .header_bar = gray(46),
      .header_title = gray(220),
      .header_subtitle = gray(160),
      .body = gray(42),
      .panel = gray(52),
      .panel_secondary = gray(48),
      .border = gray(78),
      .text_primary = gray(235),
      .text_secondary = gray(200),
      .text_muted = gray(150),
      .footer_text = gray(132),
      .footer_bar = gray(38),
      .progress_fg = gray(200),
      .progress_bg = gray(72),
  };
}

static COLORREF desaturate_to_gray(COLORREF c) {
  const BYTE r = GetRValue(c);
  const BYTE g = GetGValue(c);
  const BYTE b = GetBValue(c);
  const BYTE v = static_cast<BYTE>((static_cast<int>(r) + g + b) / 3);
  return RGB(v, v, v);
}

static ThemePalette palette_system() {
  const COLORREF btn = GetSysColor(COLOR_BTNFACE);
  const COLORREF win = GetSysColor(COLOR_WINDOW);
  const COLORREF body = desaturate_to_gray(btn);
  const COLORREF panel2 = desaturate_to_gray(RGB(
      (GetRValue(win) + GetRValue(btn)) / 2, (GetGValue(win) + GetGValue(btn)) / 2,
      (GetBValue(win) + GetBValue(btn)) / 2));

  return ThemePalette{
      .header_bar = gray(38),
      .header_title = gray(208),
      .header_subtitle = gray(142),
      .body = body,
      .panel = win,
      .panel_secondary = panel2,
      .border = GetSysColor(COLOR_3DLIGHT),
      .text_primary = GetSysColor(COLOR_WINDOWTEXT),
      .text_secondary = gray(68),
      .text_muted = gray(112),
      .footer_text = gray(90),
      .footer_bar = desaturate_to_gray(RGB(
          (GetRValue(btn) + static_cast<int>(GetRValue(win)) * 2) / 3,
          (GetGValue(btn) + static_cast<int>(GetGValue(win)) * 2) / 3,
          (GetBValue(btn) + static_cast<int>(GetBValue(win)) * 2) / 3)),
      .progress_fg = gray(44),
      .progress_bg = gray(212),
  };
}

static ThemePalette compute_palette(gui::ThemeMode mode) {
  switch (mode) {
    case gui::ThemeMode::Dark:
      return palette_dark();
    case gui::ThemeMode::Light:
      return palette_light();
    case gui::ThemeMode::System:
    default:
      return palette_system();
  }
}

static void rebuild_brushes() {
  if (g.brushes.body) DeleteObject(g.brushes.body);
  if (g.brushes.panel) DeleteObject(g.brushes.panel);
  if (g.brushes.panel_secondary) DeleteObject(g.brushes.panel_secondary);
  if (g.brushes.footer) DeleteObject(g.brushes.footer);
  g.brushes.body = CreateSolidBrush(g.pal.body);
  g.brushes.panel = CreateSolidBrush(g.pal.panel);
  g.brushes.panel_secondary = CreateSolidBrush(g.pal.panel_secondary);
  g.brushes.footer = CreateSolidBrush(g.pal.footer_bar);
}

static HFONT create_ui_font(HWND hwnd_for_dpi, int height_px, int weight) {
  HDC hdc = GetDC(hwnd_for_dpi);
  const int px = -MulDiv(height_px, GetDeviceCaps(hdc, LOGPIXELSY), 72);
  ReleaseDC(hwnd_for_dpi, hdc);
  // Segoe UI Variable ships with Windows 11; GDI falls back to Segoe UI on older systems.
  return CreateFontW(px, 0, 0, 0, weight, 0, 0, 0, DEFAULT_CHARSET, OUT_DEFAULT_PRECIS,
                     CLIP_DEFAULT_PRECIS, CLEARTYPE_QUALITY, DEFAULT_PITCH | FF_DONTCARE, L"Segoe UI Variable");
}

static void apply_fonts_to_children(HWND hwnd) {
  HWND child = GetWindow(hwnd, GW_CHILD);
  while (child) {
    if (child != g.hwnd_header && child != GetDlgItem(hwnd, IDC_LBL_CREDIT)) {
      SendMessageW(child, WM_SETFONT, reinterpret_cast<WPARAM>(g.font_ui), TRUE);
    }
    child = GetWindow(child, GW_HWNDNEXT);
  }
}

LRESULT CALLBACK header_wnd_proc(HWND hwnd, UINT msg, WPARAM wparam, LPARAM lparam) {
  switch (msg) {
    case WM_PAINT: {
      PAINTSTRUCT ps{};
      HDC hdc = BeginPaint(hwnd, &ps);
      RECT rc{};
      GetClientRect(hwnd, &rc);

      HBRUSH hb = CreateSolidBrush(g.pal.header_bar);
      FillRect(hdc, &rc, hb);
      DeleteObject(hb);

      HPEN pen = CreatePen(PS_SOLID, 1, g.pal.border);
      HGDIOBJ old_pen = SelectObject(hdc, pen);
      MoveToEx(hdc, rc.left, rc.bottom - 1, nullptr);
      LineTo(hdc, rc.right, rc.bottom - 1);
      SelectObject(hdc, old_pen);
      DeleteObject(pen);

      constexpr int pad_x = 16;
      constexpr int pad_top = 10;
      // One pixel above the bottom hairline so subtitle is not bisected by it.
      constexpr int pad_above_border = 2;

      SetBkMode(hdc, TRANSPARENT);
      if (g.font_header_title) SelectObject(hdc, g.font_header_title);
      SetTextColor(hdc, g.pal.header_title);
      RECT line1 = rc;
      line1.left += pad_x;
      line1.right -= pad_x;
      line1.top = pad_top;
      line1.bottom = line1.top + 28;
      DrawTextW(hdc, ramc::i18n::tr(ramc::i18n::StringId::HeaderProduct), -1, &line1,
                DT_LEFT | DT_SINGLELINE | DT_TOP);

      if (g.font_header_sub) SelectObject(hdc, g.font_header_sub);
      SetTextColor(hdc, g.pal.header_subtitle);
      RECT line2 = rc;
      line2.left += pad_x;
      line2.right -= pad_x;
      line2.top = line1.bottom + 3;
      line2.bottom = rc.bottom - pad_above_border;
      DrawTextW(hdc, ramc::i18n::tr(ramc::i18n::StringId::HeaderVendor), -1, &line2,
                DT_LEFT | DT_SINGLELINE | DT_VCENTER);

      EndPaint(hwnd, &ps);
      return 0;
    }
    case WM_ERASEBKGND:
      return 1;
    default:
      return DefWindowProcW(hwnd, msg, wparam, lparam);
  }
}

static void apply_theme(HWND hwnd) {
  g.pal = compute_palette(g.settings.theme);
  rebuild_brushes();

  if (g.hwnd_header) InvalidateRect(g.hwnd_header, nullptr, TRUE);

  const HWND pr = GetDlgItem(hwnd, IDC_PROGRESS);
  if (pr) {
    SendMessageW(pr, PBM_SETBARCOLOR, 0, static_cast<LPARAM>(g.pal.progress_fg));
    SendMessageW(pr, PBM_SETBKCOLOR, 0, static_cast<LPARAM>(g.pal.progress_bg));
  }

  InvalidateRect(hwnd, nullptr, TRUE);
}

void set_text(HWND hwnd, int idc, const std::wstring& text) {
  const HWND edit = GetDlgItem(hwnd, idc);
  if (!edit) return;
  SetWindowTextW(edit, text.c_str());
}

std::wstring get_text(HWND hwnd, int idc) {
  const HWND edit = GetDlgItem(hwnd, idc);
  if (!edit) return {};
  const int len = GetWindowTextLengthW(edit);
  std::wstring buf;
  buf.resize(static_cast<size_t>(len));
  GetWindowTextW(edit, buf.data(), len + 1);
  return buf;
}

void append_text(HWND hwnd, int idc, const std::wstring& text) {
  auto existing = get_text(hwnd, idc);
  if (!existing.empty() && existing.back() != L'\n') existing += L"\r\n";
  existing += text;
  set_text(hwnd, idc, existing);
}

void log_line(HWND hwnd, const std::wstring& text) {
  SYSTEMTIME st{};
  GetLocalTime(&st);
  wchar_t ts[64]{};
  _snwprintf_s(ts, _TRUNCATE, L"[%02u:%02u:%02u] ", st.wHour, st.wMinute, st.wSecond);
  append_text(hwnd, IDC_EDIT_LOG, std::wstring(ts) + text);
}

void update_admin_button(HWND hwnd) {
  g.is_admin = gui::is_running_as_admin();
  const HWND btn = GetDlgItem(hwnd, IDC_BTN_ADMIN);
  if (!btn) return;
  if (g.is_admin) {
    SetWindowTextW(btn, ramc::i18n::tr(ramc::i18n::StringId::BtnAdminLocked));
    EnableWindow(btn, FALSE);
  } else {
    SetWindowTextW(btn, ramc::i18n::tr(ramc::i18n::StringId::BtnElevate));
    EnableWindow(btn, TRUE);
  }
}

void do_status(HWND hwnd) {
  const auto st = services::get_memory_status();
  set_text(hwnd, IDC_EDIT_STATUS, ramc::i18n::format_status(st));
}

void apply_language(HWND hwnd) {
  ramc::i18n::set_language(g.settings.language);
  SetWindowTextW(hwnd, ramc::i18n::tr(ramc::i18n::StringId::WindowTitle));
  SetWindowTextW(GetDlgItem(hwnd, IDC_BTN_STATUS), ramc::i18n::tr(ramc::i18n::StringId::BtnRefresh));
  SetWindowTextW(GetDlgItem(hwnd, IDC_BTN_CLEAN_SELF), ramc::i18n::tr(ramc::i18n::StringId::BtnTrimSelf));
  SetWindowTextW(GetDlgItem(hwnd, IDC_BTN_CLEAN_ALL), ramc::i18n::tr(ramc::i18n::StringId::BtnTrimAll));
  SetWindowTextW(GetDlgItem(hwnd, IDC_BTN_SETTINGS), ramc::i18n::tr(ramc::i18n::StringId::BtnSettings));
  SetWindowTextW(GetDlgItem(hwnd, IDC_LBL_SECTION_STATUS), ramc::i18n::tr(ramc::i18n::StringId::SectionStatus));
  SetWindowTextW(GetDlgItem(hwnd, IDC_LBL_SECTION_LOG), ramc::i18n::tr(ramc::i18n::StringId::SectionLog));
  SetWindowTextW(GetDlgItem(hwnd, IDC_LBL_CREDIT), ramc::i18n::tr(ramc::i18n::StringId::FooterCredit));
  update_admin_button(hwnd);
  do_status(hwnd);
  if (g.hwnd_header) InvalidateRect(g.hwnd_header, nullptr, TRUE);
}

void set_busy(HWND hwnd, bool busy) {
  EnableWindow(GetDlgItem(hwnd, IDC_BTN_STATUS), !busy);
  EnableWindow(GetDlgItem(hwnd, IDC_BTN_CLEAN_SELF), !busy);
  EnableWindow(GetDlgItem(hwnd, IDC_BTN_CLEAN_ALL), !busy);
  EnableWindow(GetDlgItem(hwnd, IDC_BTN_SETTINGS), !busy);
  if (!g.is_admin) EnableWindow(GetDlgItem(hwnd, IDC_BTN_ADMIN), !busy);

  const HWND pr = GetDlgItem(hwnd, IDC_PROGRESS);
  if (pr) {
    ShowWindow(pr, busy ? SW_SHOW : SW_HIDE);
    SendMessageW(pr, PBM_SETPOS, 0, 0);
  }
}

void start_clean_async(HWND hwnd, bool all) {
  if (g.worker_running.exchange(true)) return;

  set_busy(hwnd, true);
  g.worker_done = 0;
  g.worker_total = 0;
  log_line(hwnd, all ? ramc::i18n::tr(ramc::i18n::StringId::MsgTrimStartAll)
                     : ramc::i18n::tr(ramc::i18n::StringId::MsgTrimStartSelf));

  std::thread([hwnd, all]() {
    auto* wr = new WorkerResult{};
    wr->before = services::get_memory_status();

    if (!all) {
      wr->clean = services::clean_working_sets({.include_all_processes = false});
      g.worker_total = 1;
      g.worker_done = 1;
      PostMessageW(hwnd, WM_APP_PROGRESS, 1, 1);
    } else {
      const auto pids = core::win::list_process_ids();
      g.worker_total = static_cast<std::uint32_t>(pids.size());

      services::CleanResult r{};
      r.attempted = static_cast<std::uint32_t>(pids.size());

      std::uint32_t done = 0;
      for (const auto pid : pids) {
        const auto tr = core::win::trim_working_set_by_pid(pid);
        if (tr == core::win::TrimResult::Ok) r.succeeded++;
        else if (tr == core::win::TrimResult::AccessDenied) r.access_denied++;
        else r.failed++;

        done++;
        g.worker_done = done;
        if ((done % 8) == 0 || done == r.attempted) {
          PostMessageW(hwnd, WM_APP_PROGRESS, done, r.attempted);
        }
      }
      wr->clean = r;
    }

    wr->after = services::get_memory_status();
    PostMessageW(hwnd, WM_APP_DONE, 0, reinterpret_cast<LPARAM>(wr));
  }).detach();
}

void layout(HWND hwnd) {
  RECT rc{};
  GetClientRect(hwnd, &rc);
  const int w = rc.right - rc.left;
  const int h = rc.bottom - rc.top;

  if (g.hwnd_header) MoveWindow(g.hwnd_header, 0, 0, w, kHeaderHeight, TRUE);

  const int pad = kContentPad;
  int y = kHeaderHeight + pad;

  MoveWindow(GetDlgItem(hwnd, IDC_BTN_STATUS), pad, y, kBtnW, kBtnH, TRUE);
  MoveWindow(GetDlgItem(hwnd, IDC_BTN_CLEAN_SELF), pad + (kBtnW + kBtnGap), y, kBtnW, kBtnH, TRUE);
  MoveWindow(GetDlgItem(hwnd, IDC_BTN_CLEAN_ALL), pad + 2 * (kBtnW + kBtnGap), y, kBtnW, kBtnH, TRUE);
  MoveWindow(GetDlgItem(hwnd, IDC_BTN_SETTINGS), pad + 3 * (kBtnW + kBtnGap), y, kBtnW, kBtnH, TRUE);
  MoveWindow(GetDlgItem(hwnd, IDC_BTN_ADMIN), pad + 4 * (kBtnW + kBtnGap), y, kBtnW + 36, kBtnH, TRUE);

  y += kBtnH + kBlockGap;

  MoveWindow(GetDlgItem(hwnd, IDC_LBL_SECTION_STATUS), pad, y, w - 2 * pad, kSectionLabelH, TRUE);
  y += kSectionLabelH + 5;

  MoveWindow(GetDlgItem(hwnd, IDC_EDIT_STATUS), pad, y, w - 2 * pad, kStatusBlockH, TRUE);
  y += kStatusBlockH + kBlockGap;

  MoveWindow(GetDlgItem(hwnd, IDC_LBL_SECTION_LOG), pad, y, w - 2 * pad, kSectionLabelH, TRUE);
  y += kSectionLabelH + 5;

  const int progress_top = h - pad - kProgressH;
  const int credit_top = progress_top - kFooterAboveProgress - kCreditH;
  const int log_h = std::max(72, credit_top - kGapAfterLog - y);

  g.footer_strip_top = credit_top;

  MoveWindow(GetDlgItem(hwnd, IDC_EDIT_LOG), pad, y, w - 2 * pad, log_h, TRUE);
  MoveWindow(GetDlgItem(hwnd, IDC_LBL_CREDIT), pad, credit_top, w - 2 * pad, kCreditH, TRUE);
  MoveWindow(GetDlgItem(hwnd, IDC_PROGRESS), pad, progress_top, w - 2 * pad, kProgressH, TRUE);
}

struct SettingsDialogState {
  HWND owner{};
  HWND dlg{};
  HWND combo_lang{};
  HWND combo_theme{};
  HWND edit_refresh{};
  gui::Settings s{};
  bool accepted = false;
  ThemePalette pal{};
  HBRUSH bg_brush = nullptr;
  HBRUSH panel_brush = nullptr;
};

LRESULT CALLBACK settings_dlg_proc(HWND hwnd, UINT msg, WPARAM wparam, LPARAM lparam) {
  auto* st = reinterpret_cast<SettingsDialogState*>(GetWindowLongPtrW(hwnd, GWLP_USERDATA));

  switch (msg) {
    case WM_CREATE: {
      auto* cs = reinterpret_cast<CREATESTRUCTW*>(lparam);
      st = reinterpret_cast<SettingsDialogState*>(cs->lpCreateParams);
      SetWindowLongPtrW(hwnd, GWLP_USERDATA, reinterpret_cast<LONG_PTR>(st));
      st->dlg = hwnd;
      st->pal = compute_palette(st->s.theme);
      st->bg_brush = CreateSolidBrush(st->pal.body);
      st->panel_brush = CreateSolidBrush(st->pal.panel);

      ramc::i18n::set_language(st->s.language);

      const int pad = 14;
      const int label_w = 150;
      const int ctrl_w = 230;
      const int row_h = 28;
      const int btn_w = 96;
      const int btn_h = 30;
      int y = pad;

      CreateWindowExW(0, L"STATIC", ramc::i18n::tr(ramc::i18n::StringId::SettingsLanguage), WS_CHILD | WS_VISIBLE,
                      pad, y, label_w, row_h, hwnd, nullptr, GetModuleHandleW(nullptr), nullptr);

      st->combo_lang = CreateWindowExW(
          0, WC_COMBOBOXW, L"", WS_CHILD | WS_VISIBLE | CBS_DROPDOWNLIST,
          pad + label_w, y - 2, ctrl_w, 200, hwnd, reinterpret_cast<HMENU>(1003), GetModuleHandleW(nullptr), nullptr);
      SendMessageW(st->combo_lang, CB_ADDSTRING, 0, reinterpret_cast<LPARAM>(L"English"));
      SendMessageW(st->combo_lang, CB_ADDSTRING, 0, reinterpret_cast<LPARAM>(L"\u0420\u0443\u0441\u0441\u043a\u0438\u0439"));
      SendMessageW(st->combo_lang, CB_ADDSTRING, 0, reinterpret_cast<LPARAM>(L"Deutsch"));
      SendMessageW(st->combo_lang, CB_SETCURSEL, static_cast<WPARAM>(st->s.language), 0);

      y += row_h + 12;

      CreateWindowExW(0, L"STATIC", ramc::i18n::tr(ramc::i18n::StringId::SettingsPreferences), WS_CHILD | WS_VISIBLE,
                      pad, y, label_w + ctrl_w, 18, hwnd, nullptr, GetModuleHandleW(nullptr), nullptr);

      y += 22;
      CreateWindowExW(0, L"STATIC", ramc::i18n::tr(ramc::i18n::StringId::SettingsTheme), WS_CHILD | WS_VISIBLE,
                      pad, y, label_w, row_h, hwnd, nullptr, GetModuleHandleW(nullptr), nullptr);

      st->combo_theme = CreateWindowExW(
          0, WC_COMBOBOXW, L"", WS_CHILD | WS_VISIBLE | CBS_DROPDOWNLIST,
          pad + label_w, y - 2, ctrl_w, 200, hwnd, reinterpret_cast<HMENU>(1001), GetModuleHandleW(nullptr), nullptr);
      SendMessageW(st->combo_theme, CB_ADDSTRING, 0,
                   reinterpret_cast<LPARAM>(ramc::i18n::tr(ramc::i18n::StringId::ThemeMatchSystem)));
      SendMessageW(st->combo_theme, CB_ADDSTRING, 0,
                   reinterpret_cast<LPARAM>(ramc::i18n::tr(ramc::i18n::StringId::ThemeLight)));
      SendMessageW(st->combo_theme, CB_ADDSTRING, 0,
                   reinterpret_cast<LPARAM>(ramc::i18n::tr(ramc::i18n::StringId::ThemeDark)));
      SendMessageW(st->combo_theme, CB_SETCURSEL, static_cast<WPARAM>(st->s.theme), 0);

      y += row_h + 10;
      CreateWindowExW(0, L"STATIC", ramc::i18n::tr(ramc::i18n::StringId::SettingsRefreshInterval),
                      WS_CHILD | WS_VISIBLE, pad, y, label_w, row_h, hwnd, nullptr, GetModuleHandleW(nullptr), nullptr);

      st->edit_refresh = CreateWindowExW(WS_EX_CLIENTEDGE, L"EDIT", L"",
                                         WS_CHILD | WS_VISIBLE | ES_NUMBER,
                                         pad + label_w, y, 88, row_h,
                                         hwnd, reinterpret_cast<HMENU>(1002), GetModuleHandleW(nullptr), nullptr);
      wchar_t buf[32]{};
      _snwprintf_s(buf, _TRUNCATE, L"%u", static_cast<unsigned>(st->s.refresh_seconds));
      SetWindowTextW(st->edit_refresh, buf);

      const int yb = y + row_h + 18;
      CreateWindowExW(0, L"BUTTON", ramc::i18n::tr(ramc::i18n::StringId::SettingsOk), WS_CHILD | WS_VISIBLE | BS_DEFPUSHBUTTON,
                      pad + label_w + ctrl_w - btn_w * 2 - 10, yb, btn_w, btn_h,
                      hwnd, reinterpret_cast<HMENU>(IDOK), GetModuleHandleW(nullptr), nullptr);
      CreateWindowExW(0, L"BUTTON", ramc::i18n::tr(ramc::i18n::StringId::SettingsCancel), WS_CHILD | WS_VISIBLE | BS_PUSHBUTTON,
                      pad + label_w + ctrl_w - btn_w, yb, btn_w, btn_h,
                      hwnd, reinterpret_cast<HMENU>(IDCANCEL), GetModuleHandleW(nullptr), nullptr);

      return 0;
    }

    case WM_CTLCOLORSTATIC: {
      if (!st || !st->panel_brush) break;
      HDC hdc = reinterpret_cast<HDC>(wparam);
      SetTextColor(hdc, st->pal.text_primary);
      SetBkColor(hdc, st->pal.panel);
      return reinterpret_cast<LRESULT>(st->panel_brush);
    }

    case WM_CTLCOLOREDIT: {
      if (!st || !st->panel_brush) break;
      HDC hdc = reinterpret_cast<HDC>(wparam);
      SetTextColor(hdc, st->pal.text_primary);
      SetBkColor(hdc, st->pal.panel);
      return reinterpret_cast<LRESULT>(st->panel_brush);
    }

    case WM_COMMAND: {
      const int id = LOWORD(wparam);
      if (id == IDOK) {
        const LRESULT lsel = SendMessageW(st->combo_lang, CB_GETCURSEL, 0, 0);
        st->s.language = static_cast<gui::Language>(std::clamp(static_cast<int>(lsel), 0, 2));

        const LRESULT sel = SendMessageW(st->combo_theme, CB_GETCURSEL, 0, 0);
        st->s.theme = static_cast<gui::ThemeMode>(static_cast<int>(sel));

        wchar_t buf[64]{};
        GetWindowTextW(st->edit_refresh, buf, 64);
        unsigned v = 0;
        swscanf_s(buf, L"%u", &v);
        if (v < 1) v = 1;
        if (v > 3600) v = 3600;
        st->s.refresh_seconds = v;

        st->accepted = true;
        DestroyWindow(hwnd);
        return 0;
      }
      if (id == IDCANCEL) {
        DestroyWindow(hwnd);
        return 0;
      }
      return 0;
    }

    case WM_CLOSE:
      DestroyWindow(hwnd);
      return 0;

    case WM_ERASEBKGND:
      if (st && st->bg_brush) {
        RECT rc{};
        GetClientRect(hwnd, &rc);
        FillRect(reinterpret_cast<HDC>(wparam), &rc, st->bg_brush);
        return 1;
      }
      break;

    case WM_DESTROY:
      if (st && st->bg_brush) {
        DeleteObject(st->bg_brush);
        st->bg_brush = nullptr;
      }
      if (st && st->panel_brush) {
        DeleteObject(st->panel_brush);
        st->panel_brush = nullptr;
      }
      return 0;
  }

  return DefWindowProcW(hwnd, msg, wparam, lparam);
}

bool show_settings_dialog(HWND owner, gui::Settings& inout) {
  SettingsDialogState st{};
  st.owner = owner;
  st.s = inout;

  ramc::i18n::set_language(inout.language);

  HWND dlg = CreateWindowExW(
      WS_EX_DLGMODALFRAME, kSettingsDlgClass, ramc::i18n::tr(ramc::i18n::StringId::SettingsDlgTitle),
      WS_CAPTION | WS_SYSMENU,
      CW_USEDEFAULT, CW_USEDEFAULT, 460, 310,
      owner, nullptr, GetModuleHandleW(nullptr), &st);

  if (!dlg) return false;

  RECT orc{}, drc{};
  GetWindowRect(owner, &orc);
  GetWindowRect(dlg, &drc);
  const int x = orc.left + ((orc.right - orc.left) - (drc.right - drc.left)) / 2;
  const int y = orc.top + ((orc.bottom - orc.top) - (drc.bottom - drc.top)) / 2;
  SetWindowPos(dlg, nullptr, x, y, 0, 0, SWP_NOZORDER | SWP_NOSIZE);

  ShowWindow(dlg, SW_SHOW);
  EnableWindow(owner, FALSE);

  MSG msg{};
  while (IsWindow(dlg) && GetMessageW(&msg, nullptr, 0, 0) > 0) {
    if (!IsDialogMessageW(dlg, &msg)) {
      TranslateMessage(&msg);
      DispatchMessageW(&msg);
    }
  }

  EnableWindow(owner, TRUE);
  SetActiveWindow(owner);

  if (st.accepted) inout = st.s;
  return st.accepted;
}

LRESULT CALLBACK wnd_proc(HWND hwnd, UINT msg, WPARAM wparam, LPARAM lparam) {
  switch (msg) {
    case WM_CREATE: {
      InitCommonControls();

      g.settings = gui::load_settings();
      ramc::i18n::set_language(g.settings.language);

      g.font_ui = create_ui_font(hwnd, 9, FW_NORMAL);
      g.font_ui_semibold = create_ui_font(hwnd, 9, FW_SEMIBOLD);
      g.font_header_title = create_ui_font(hwnd, 17, FW_SEMIBOLD);
      g.font_header_sub = create_ui_font(hwnd, 11, FW_NORMAL);

      g.hwnd_header = CreateWindowExW(
          0, kHeaderClass, L"", WS_CHILD | WS_VISIBLE,
          0, 0, 0, kHeaderHeight, hwnd, nullptr, GetModuleHandleW(nullptr), nullptr);

      CreateWindowExW(
          0, L"BUTTON", ramc::i18n::tr(ramc::i18n::StringId::BtnRefresh), WS_CHILD | WS_VISIBLE | BS_PUSHBUTTON,
          0, 0, 0, 0, hwnd, reinterpret_cast<HMENU>(static_cast<intptr_t>(IDC_BTN_STATUS)),
          GetModuleHandleW(nullptr), nullptr);

      CreateWindowExW(
          0, L"BUTTON", ramc::i18n::tr(ramc::i18n::StringId::BtnTrimSelf), WS_CHILD | WS_VISIBLE | BS_PUSHBUTTON,
          0, 0, 0, 0, hwnd, reinterpret_cast<HMENU>(static_cast<intptr_t>(IDC_BTN_CLEAN_SELF)),
          GetModuleHandleW(nullptr), nullptr);

      CreateWindowExW(
          0, L"BUTTON", ramc::i18n::tr(ramc::i18n::StringId::BtnTrimAll), WS_CHILD | WS_VISIBLE | BS_PUSHBUTTON,
          0, 0, 0, 0, hwnd, reinterpret_cast<HMENU>(static_cast<intptr_t>(IDC_BTN_CLEAN_ALL)),
          GetModuleHandleW(nullptr), nullptr);

      CreateWindowExW(
          0, L"BUTTON", ramc::i18n::tr(ramc::i18n::StringId::BtnSettings), WS_CHILD | WS_VISIBLE | BS_PUSHBUTTON,
          0, 0, 0, 0, hwnd, reinterpret_cast<HMENU>(static_cast<intptr_t>(IDC_BTN_SETTINGS)),
          GetModuleHandleW(nullptr), nullptr);

      CreateWindowExW(
          0, L"BUTTON", ramc::i18n::tr(ramc::i18n::StringId::BtnElevate), WS_CHILD | WS_VISIBLE | BS_PUSHBUTTON,
          0, 0, 0, 0, hwnd, reinterpret_cast<HMENU>(static_cast<intptr_t>(IDC_BTN_ADMIN)),
          GetModuleHandleW(nullptr), nullptr);

      CreateWindowExW(
          0, L"STATIC", ramc::i18n::tr(ramc::i18n::StringId::SectionStatus), WS_CHILD | WS_VISIBLE | SS_LEFT,
          0, 0, 0, 0, hwnd, reinterpret_cast<HMENU>(static_cast<intptr_t>(IDC_LBL_SECTION_STATUS)),
          GetModuleHandleW(nullptr), nullptr);

      CreateWindowExW(
          0, L"STATIC", ramc::i18n::tr(ramc::i18n::StringId::SectionLog), WS_CHILD | WS_VISIBLE | SS_LEFT,
          0, 0, 0, 0, hwnd, reinterpret_cast<HMENU>(static_cast<intptr_t>(IDC_LBL_SECTION_LOG)),
          GetModuleHandleW(nullptr), nullptr);

      CreateWindowExW(
          0, L"STATIC", ramc::i18n::tr(ramc::i18n::StringId::FooterCredit),
          WS_CHILD | WS_VISIBLE | SS_RIGHT | SS_CENTERIMAGE,
          0, 0, 0, 0, hwnd, reinterpret_cast<HMENU>(static_cast<intptr_t>(IDC_LBL_CREDIT)),
          GetModuleHandleW(nullptr), nullptr);

      const DWORD status_style = WS_CHILD | WS_VISIBLE | WS_VSCROLL | ES_LEFT | ES_MULTILINE |
                                 ES_AUTOVSCROLL | ES_READONLY;
      CreateWindowExW(
          WS_EX_CLIENTEDGE, L"EDIT", L"", status_style,
          0, 0, 0, 0, hwnd, reinterpret_cast<HMENU>(static_cast<intptr_t>(IDC_EDIT_STATUS)),
          GetModuleHandleW(nullptr), nullptr);

      const DWORD log_style = WS_CHILD | WS_VISIBLE | WS_VSCROLL | ES_LEFT | ES_MULTILINE |
                              ES_AUTOVSCROLL | ES_READONLY;
      CreateWindowExW(
          WS_EX_CLIENTEDGE, L"EDIT", L"", log_style,
          0, 0, 0, 0, hwnd, reinterpret_cast<HMENU>(static_cast<intptr_t>(IDC_EDIT_LOG)),
          GetModuleHandleW(nullptr), nullptr);

      CreateWindowExW(
          0, PROGRESS_CLASSW, L"", WS_CHILD | PBS_SMOOTH,
          0, 0, 0, 0, hwnd, reinterpret_cast<HMENU>(static_cast<intptr_t>(IDC_PROGRESS)),
          GetModuleHandleW(nullptr), nullptr);

      apply_theme(hwnd);
      SendMessageW(GetDlgItem(hwnd, IDC_LBL_SECTION_STATUS), WM_SETFONT, reinterpret_cast<WPARAM>(g.font_ui_semibold), TRUE);
      SendMessageW(GetDlgItem(hwnd, IDC_LBL_SECTION_LOG), WM_SETFONT, reinterpret_cast<WPARAM>(g.font_ui_semibold), TRUE);
      SendMessageW(GetDlgItem(hwnd, IDC_LBL_CREDIT), WM_SETFONT, reinterpret_cast<WPARAM>(g.font_ui), TRUE);
      apply_fonts_to_children(hwnd);

      update_admin_button(hwnd);

      layout(hwnd);
      do_status(hwnd);
      log_line(hwnd, ramc::i18n::tr(ramc::i18n::StringId::MsgAppStarted));

      const UINT refresh_ms = static_cast<UINT>(g.settings.refresh_seconds * 1000u);
      SetTimer(hwnd, IDT_AUTOREFRESH, refresh_ms, nullptr);
      set_busy(hwnd, false);
      return 0;
    }

    case WM_SIZE: {
      layout(hwnd);
      return 0;
    }

    case WM_TIMER: {
      if (wparam == IDT_AUTOREFRESH) {
        if (!g.worker_running.load()) do_status(hwnd);
        return 0;
      }
      return 0;
    }

    case WM_CTLCOLORSTATIC: {
      const HWND ctl = reinterpret_cast<HWND>(lparam);
      if (ctl == GetDlgItem(hwnd, IDC_LBL_CREDIT)) {
        HDC hdc = reinterpret_cast<HDC>(wparam);
        SetBkMode(hdc, TRANSPARENT);
        SetTextColor(hdc, g.pal.footer_text);
        SetBkColor(hdc, g.pal.footer_bar);
        return reinterpret_cast<LRESULT>(g.brushes.footer);
      }
      if (ctl == GetDlgItem(hwnd, IDC_LBL_SECTION_STATUS) || ctl == GetDlgItem(hwnd, IDC_LBL_SECTION_LOG)) {
        HDC hdc = reinterpret_cast<HDC>(wparam);
        SetBkMode(hdc, TRANSPARENT);
        SetTextColor(hdc, g.pal.text_muted);
        return reinterpret_cast<LRESULT>(g.brushes.body);
      }
      break;
    }

    case WM_CTLCOLOREDIT: {
      HDC hdc = reinterpret_cast<HDC>(wparam);
      const HWND ctl = reinterpret_cast<HWND>(lparam);
      if (ctl == GetDlgItem(hwnd, IDC_EDIT_STATUS)) {
        SetTextColor(hdc, g.pal.text_primary);
        SetBkColor(hdc, g.pal.panel);
        return reinterpret_cast<LRESULT>(g.brushes.panel);
      }
      if (ctl == GetDlgItem(hwnd, IDC_EDIT_LOG)) {
        SetTextColor(hdc, g.pal.text_secondary);
        SetBkColor(hdc, g.pal.panel_secondary);
        return reinterpret_cast<LRESULT>(g.brushes.panel_secondary);
      }
      break;
    }

    case WM_COMMAND: {
      const int id = LOWORD(wparam);
      if (id == IDC_BTN_STATUS) {
        do_status(hwnd);
        return 0;
      }
      if (id == IDC_BTN_CLEAN_SELF) {
        start_clean_async(hwnd, false);
        return 0;
      }
      if (id == IDC_BTN_CLEAN_ALL) {
        start_clean_async(hwnd, true);
        return 0;
      }
      if (id == IDC_BTN_SETTINGS) {
        auto next = g.settings;
        if (show_settings_dialog(hwnd, next)) {
          g.settings = next;
          gui::save_settings(g.settings);
          apply_theme(hwnd);
          apply_language(hwnd);
          apply_fonts_to_children(hwnd);
          KillTimer(hwnd, IDT_AUTOREFRESH);
          const UINT refresh_ms = static_cast<UINT>(g.settings.refresh_seconds * 1000u);
          SetTimer(hwnd, IDT_AUTOREFRESH, refresh_ms, nullptr);
          log_line(hwnd, ramc::i18n::tr(ramc::i18n::StringId::MsgSettingsSaved));
        }
        return 0;
      }
      if (id == IDC_BTN_ADMIN) {
        log_line(hwnd, ramc::i18n::tr(ramc::i18n::StringId::MsgUacRequest));
        if (gui::relaunch_as_admin()) {
          PostQuitMessage(0);
        } else {
          log_line(hwnd, ramc::i18n::tr(ramc::i18n::StringId::MsgUacFailed));
        }
        return 0;
      }
      return 0;
    }

    case WM_APP_PROGRESS: {
      const std::uint32_t done = static_cast<std::uint32_t>(wparam);
      const std::uint32_t total = static_cast<std::uint32_t>(lparam);
      const HWND pr = GetDlgItem(hwnd, IDC_PROGRESS);
      if (pr && total > 0) {
        SendMessageW(pr, PBM_SETRANGE32, 0, static_cast<LPARAM>(total));
        SendMessageW(pr, PBM_SETPOS, static_cast<WPARAM>(done), 0);
      }
      return 0;
    }

    case WM_APP_DONE: {
      auto* wr = reinterpret_cast<WorkerResult*>(lparam);
      g.worker_running = false;

      log_line(hwnd, ramc::i18n::msg_trim_complete(wr->clean));

      set_text(hwnd, IDC_EDIT_STATUS, ramc::i18n::format_status(wr->after));

      delete wr;
      set_busy(hwnd, false);
      return 0;
    }

    case WM_ERASEBKGND: {
      HDC hdc = reinterpret_cast<HDC>(wparam);
      RECT rc{};
      GetClientRect(hwnd, &rc);
      RECT top = rc;
      if (g.footer_strip_top > 0 && g.footer_strip_top < rc.bottom) {
        top.bottom = g.footer_strip_top;
      }
      FillRect(hdc, &top, g.brushes.body);
      if (g.footer_strip_top > 0 && g.footer_strip_top < rc.bottom && g.brushes.footer) {
        RECT fr = rc;
        fr.top = g.footer_strip_top;
        FillRect(hdc, &fr, g.brushes.footer);
      }
      return 1;
    }

    case WM_DESTROY:
      KillTimer(hwnd, IDT_AUTOREFRESH);
      if (g.brushes.body) DeleteObject(g.brushes.body);
      if (g.brushes.panel) DeleteObject(g.brushes.panel);
      if (g.brushes.panel_secondary) DeleteObject(g.brushes.panel_secondary);
      if (g.brushes.footer) DeleteObject(g.brushes.footer);
      if (g.font_ui) DeleteObject(g.font_ui);
      if (g.font_ui_semibold) DeleteObject(g.font_ui_semibold);
      if (g.font_header_title) DeleteObject(g.font_header_title);
      if (g.font_header_sub) DeleteObject(g.font_header_sub);
      PostQuitMessage(0);
      return 0;
  }
  return DefWindowProcW(hwnd, msg, wparam, lparam);
}

}  // namespace

int WINAPI wWinMain(HINSTANCE hInstance, HINSTANCE, PWSTR, int nCmdShow) {
  const gui::Settings boot_settings = gui::load_settings();
  ramc::i18n::set_language(boot_settings.language);

  WNDCLASSEXW wc{};
  wc.cbSize = sizeof(wc);
  wc.lpfnWndProc = wnd_proc;
  wc.hInstance = hInstance;
  wc.hCursor = LoadCursorW(nullptr, IDC_ARROW);
  wc.hbrBackground = nullptr;
  wc.lpszClassName = kAppClass;
  wc.hIcon = LoadIconW(nullptr, IDI_APPLICATION);
  wc.hIconSm = LoadIconW(nullptr, IDI_APPLICATION);

  WNDCLASSEXW wc_header{};
  wc_header.cbSize = sizeof(wc_header);
  wc_header.lpfnWndProc = header_wnd_proc;
  wc_header.hInstance = hInstance;
  wc_header.hCursor = LoadCursorW(nullptr, IDC_ARROW);
  wc_header.hbrBackground = nullptr;
  wc_header.lpszClassName = kHeaderClass;

  WNDCLASSEXW wc_settings{};
  wc_settings.cbSize = sizeof(wc_settings);
  wc_settings.lpfnWndProc = settings_dlg_proc;
  wc_settings.hInstance = hInstance;
  wc_settings.hCursor = LoadCursorW(nullptr, IDC_ARROW);
  wc_settings.hbrBackground = reinterpret_cast<HBRUSH>(COLOR_WINDOW + 1);
  wc_settings.lpszClassName = kSettingsDlgClass;

  if (!RegisterClassExW(&wc) || !RegisterClassExW(&wc_header) || !RegisterClassExW(&wc_settings)) {
    MessageBoxW(nullptr, ramc::i18n::tr(ramc::i18n::StringId::ErrRegisterClass),
                ramc::i18n::tr(ramc::i18n::StringId::WindowTitle), MB_ICONERROR | MB_OK);
    return 1;
  }

  HWND hwnd = CreateWindowExW(
      0, kAppClass, ramc::i18n::tr(ramc::i18n::StringId::WindowTitle),
      WS_OVERLAPPEDWINDOW,
      CW_USEDEFAULT, CW_USEDEFAULT, 760, 560,
      nullptr, nullptr, hInstance, nullptr);

  if (!hwnd) {
    MessageBoxW(nullptr, ramc::i18n::tr(ramc::i18n::StringId::ErrCreateWindow),
                ramc::i18n::tr(ramc::i18n::StringId::WindowTitle), MB_ICONERROR | MB_OK);
    return 1;
  }

  ShowWindow(hwnd, nCmdShow);
  UpdateWindow(hwnd);

  MSG msg{};
  while (GetMessageW(&msg, nullptr, 0, 0) > 0) {
    TranslateMessage(&msg);
    DispatchMessageW(&msg);
  }
  return static_cast<int>(msg.wParam);
}
