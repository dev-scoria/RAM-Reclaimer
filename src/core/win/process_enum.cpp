#include "core/win/process_enum.h"

#include <windows.h>
#include <psapi.h>

#include <stdexcept>

namespace core::win {

std::vector<std::uint32_t> list_process_ids() {
  DWORD bytes_needed = 0;
  std::vector<DWORD> pids(4096);

  if (!EnumProcesses(pids.data(), static_cast<DWORD>(pids.size() * sizeof(DWORD)), &bytes_needed)) {
    throw std::runtime_error("EnumProcesses failed");
  }

  const auto count = bytes_needed / sizeof(DWORD);
  pids.resize(count);

  std::vector<std::uint32_t> out;
  out.reserve(pids.size());
  for (DWORD pid : pids) {
    if (pid == 0) continue;
    out.push_back(static_cast<std::uint32_t>(pid));
  }
  return out;
}

}  // namespace core::win

