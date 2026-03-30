#include "core/win/memory_status.h"

#include <windows.h>

#include <stdexcept>

namespace core::win {

MemoryStatus query_memory_status() {
  MEMORYSTATUSEX ms{};
  ms.dwLength = sizeof(ms);
  if (!GlobalMemoryStatusEx(&ms)) {
    throw std::runtime_error("GlobalMemoryStatusEx failed");
  }

  MemoryStatus out{};
  out.total_phys_bytes = static_cast<std::uint64_t>(ms.ullTotalPhys);
  out.avail_phys_bytes = static_cast<std::uint64_t>(ms.ullAvailPhys);
  out.total_pagefile_bytes = static_cast<std::uint64_t>(ms.ullTotalPageFile);
  out.avail_pagefile_bytes = static_cast<std::uint64_t>(ms.ullAvailPageFile);
  out.memory_load_percent = static_cast<std::uint32_t>(ms.dwMemoryLoad);
  return out;
}

}  // namespace core::win

