// SPDX-License-Identifier: MIT
// Copyright (c) Scoria Developers Portal. See LICENSE.

#include "gui/localization.h"

#include "services/cleaner.h"

#include <cstdio>
#include <sstream>

namespace ramc::i18n {
namespace {

gui::Language g_lang = gui::Language::English;

// Order must match enum StringId exactly.
static const wchar_t* const kEn[static_cast<int>(StringId::Count)] = {
    L"RAMC — Memory Management · Scoria Developers Portal",
    L"RAMC",
    L"by Scoria Developers Portal",
    L"RAMC  |  by Scoria Developers Portal",
    L"STATUS",
    L"LOG",
    L"Refresh",
    L"Trim (self)",
    L"Trim (all)",
    L"Settings",
    L"Elevate",
    L"Administrator",
    L"Settings — RAMC",
    L"Preferences",
    L"Theme",
    L"Match system",
    L"Light (grayscale)",
    L"Dark (grayscale)",
    L"Refresh interval (seconds)",
    L"OK",
    L"Cancel",
    L"Language",
    L"Application started",
    L"Settings saved",
    L"Requesting elevation (UAC)…",
    L"Elevation was cancelled or failed",
    L"Working-set trim (current process) started",
    L"Working-set trim (all processes) started",
    L"Physical memory   %.2f GiB total   ·   %.2f GiB available\r\n",
    L"Paging file       %.2f GiB total   ·   %.2f GiB available\r\n",
    L"Memory load       %u%%\r\n",
    L"Trim complete. Attempted %u, succeeded %u, access denied %u, failed %u",
    L"RegisterClassExW failed",
    L"CreateWindowExW failed",
};

static const wchar_t* const kRu[static_cast<int>(StringId::Count)] = {
    L"RAMC — управление памятью · Scoria Developers Portal",
    L"RAMC",
    L"by Scoria Developers Portal",
    L"RAMC  |  by Scoria Developers Portal",
    L"СТАТУС",
    L"ЖУРНАЛ",
    L"Обновить",
    L"Обрезка (текущий процесс)",
    L"Обрезка (все процессы)",
    L"Параметры",
    L"Повысить права",
    L"Администратор",
    L"Параметры — RAMC",
    L"Оформление",
    L"Тема",
    L"Как в системе",
    L"Светлая (серая)",
    L"Тёмная (серая)",
    L"Интервал обновления (сек)",
    L"OK",
    L"Отмена",
    L"Язык",
    L"Приложение запущено",
    L"Параметры сохранены",
    L"Запрос повышения прав (UAC)…",
    L"Повышение отменено или не удалось",
    L"Запущена обрезка рабочего набора (текущий процесс)",
    L"Запущена обрезка рабочего набора (все процессы)",
    L"Физическая память   %.2f ГиБ всего   ·   %.2f ГиБ доступно\r\n",
    L"Файл подкачки       %.2f ГиБ всего   ·   %.2f ГиБ доступно\r\n",
    L"Загрузка памяти     %u%%\r\n",
    L"Обрезка завершена. Попыток: %u, успешно: %u, отказано: %u, ошибок: %u",
    L"RegisterClassExW не удалась",
    L"CreateWindowExW не удалась",
};

static const wchar_t* const kDe[static_cast<int>(StringId::Count)] = {
    L"RAMC — Speicherverwaltung · Scoria Developers Portal",
    L"RAMC",
    L"by Scoria Developers Portal",
    L"RAMC  |  by Scoria Developers Portal",
    L"STATUS",
    L"PROTOKOLL",
    L"Aktualisieren",
    L"Trim (dieser Prozess)",
    L"Trim (alle Prozesse)",
    L"Einstellungen",
    L"Rechte erhöhen",
    L"Administrator",
    L"Einstellungen — RAMC",
    L"Darstellung",
    L"Thema",
    L"Wie System",
    L"Hell (Graustufen)",
    L"Dunkel (Graustufen)",
    L"Aktualisierungsintervall (Sekunden)",
    L"OK",
    L"Abbrechen",
    L"Sprache",
    L"Anwendung gestartet",
    L"Einstellungen gespeichert",
    L"Rechteerhöhung (UAC) wird angefordert…",
    L"Rechteerhöhung abgebrochen oder fehlgeschlagen",
    L"Working-Set-Kürzung (aktueller Prozess) gestartet",
    L"Working-Set-Kürzung (alle Prozesse) gestartet",
    L"Physischer Speicher   %.2f GiB gesamt   ·   %.2f GiB verfügbar\r\n",
    L"Auslagerungsdatei     %.2f GiB gesamt   ·   %.2f GiB verfügbar\r\n",
    L"Speicherauslastung    %u%%\r\n",
    L"Trim abgeschlossen. Versucht: %u, erfolgreich: %u, Zugriff verweigert: %u, Fehler: %u",
    L"RegisterClassExW fehlgeschlagen",
    L"CreateWindowExW fehlgeschlagen",
};

}  // namespace

void set_language(gui::Language lang) { g_lang = lang; }

const wchar_t* tr(StringId id) {
  const int i = static_cast<int>(id);
  if (i < 0 || i >= static_cast<int>(StringId::Count)) return L"";
  switch (g_lang) {
    case gui::Language::Russian:
      return kRu[i];
    case gui::Language::German:
      return kDe[i];
    default:
      return kEn[i];
  }
}

std::wstring format_status(const services::MemoryStatus& st) {
  auto to_gib = [](std::uint64_t bytes) -> double {
    return static_cast<double>(bytes) / (1024.0 * 1024.0 * 1024.0);
  };
  wchar_t b1[384]{};
  wchar_t b2[384]{};
  wchar_t b3[128]{};
  _snwprintf_s(b1, _TRUNCATE, tr(StringId::FmtPhysicalLine), to_gib(st.total_phys_bytes), to_gib(st.avail_phys_bytes));
  _snwprintf_s(b2, _TRUNCATE, tr(StringId::FmtPagefileLine), to_gib(st.total_pagefile_bytes),
               to_gib(st.avail_pagefile_bytes));
  _snwprintf_s(b3, _TRUNCATE, tr(StringId::FmtLoadLine), static_cast<unsigned>(st.memory_load_percent));
  std::wstring out;
  out += b1;
  out += b2;
  out += b3;
  return out;
}

std::wstring msg_trim_complete(const services::CleanResult& r) {
  wchar_t buf[512]{};
  _snwprintf_s(buf, _TRUNCATE, tr(StringId::FmtTrimComplete), static_cast<unsigned>(r.attempted),
               static_cast<unsigned>(r.succeeded), static_cast<unsigned>(r.access_denied),
               static_cast<unsigned>(r.failed));
  return buf;
}

}  // namespace ramc::i18n
