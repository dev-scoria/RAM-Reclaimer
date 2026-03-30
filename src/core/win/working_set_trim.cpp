#include "core/win/working_set_trim.h"

#include <windows.h>
#include <psapi.h>

namespace core::win {

static TrimResult trim_handle(HANDLE h) {
  if (!h) return TrimResult::Failed;

  if (EmptyWorkingSet(h)) return TrimResult::Ok;

  const DWORD err = GetLastError();
  if (err == ERROR_ACCESS_DENIED) return TrimResult::AccessDenied;
  return TrimResult::Failed;
}

TrimResult trim_working_set_by_pid(std::uint32_t pid) {
  const DWORD access = PROCESS_QUERY_LIMITED_INFORMATION | PROCESS_SET_QUOTA;
  HANDLE h = OpenProcess(access, FALSE, static_cast<DWORD>(pid));
  if (!h) {
    const DWORD err = GetLastError();
    if (err == ERROR_ACCESS_DENIED) return TrimResult::AccessDenied;
    return TrimResult::Failed;
  }

  const auto res = trim_handle(h);
  CloseHandle(h);
  return res;
}

}  // namespace core::win

