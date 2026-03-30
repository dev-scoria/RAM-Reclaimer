#pragma once

#include <cstdint>

namespace services {

struct MemoryStatus {
  std::uint64_t total_phys_bytes{};
  std::uint64_t avail_phys_bytes{};
  std::uint64_t total_pagefile_bytes{};
  std::uint64_t avail_pagefile_bytes{};
  std::uint32_t memory_load_percent{};
};

MemoryStatus get_memory_status();

struct CleanOptions {
  bool include_all_processes = false;  // otherwise: self only
};

struct CleanResult {
  std::uint32_t attempted{};
  std::uint32_t succeeded{};
  std::uint32_t access_denied{};
  std::uint32_t failed{};
};

CleanResult clean_working_sets(const CleanOptions& opt);

}  // namespace services

