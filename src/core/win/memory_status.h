#pragma once

#include <cstdint>

namespace core::win {

struct MemoryStatus {
  std::uint64_t total_phys_bytes{};
  std::uint64_t avail_phys_bytes{};
  std::uint64_t total_pagefile_bytes{};
  std::uint64_t avail_pagefile_bytes{};
  std::uint32_t memory_load_percent{};
};

MemoryStatus query_memory_status();

}  // namespace core::win

