// SPDX-License-Identifier: MIT
// Copyright (c) Scoria Developers Portal. See LICENSE.

#include "services/cleaner.h"

#include "core/win/memory_status.h"
#include "core/win/process_enum.h"
#include "core/win/working_set_trim.h"

#include <windows.h>

namespace services {

MemoryStatus get_memory_status() {
  const auto st = core::win::query_memory_status();
  return MemoryStatus{
      .total_phys_bytes = st.total_phys_bytes,
      .avail_phys_bytes = st.avail_phys_bytes,
      .total_pagefile_bytes = st.total_pagefile_bytes,
      .avail_pagefile_bytes = st.avail_pagefile_bytes,
      .memory_load_percent = st.memory_load_percent,
  };
}

CleanResult clean_working_sets(const CleanOptions& opt) {
  CleanResult r{};

  if (!opt.include_all_processes) {
    r.attempted = 1;
    const auto tr = core::win::trim_working_set_by_pid(GetCurrentProcessId());
    if (tr == core::win::TrimResult::Ok) r.succeeded++;
    else if (tr == core::win::TrimResult::AccessDenied) r.access_denied++;
    else r.failed++;
    return r;
  }

  const auto pids = core::win::list_process_ids();
  r.attempted = static_cast<std::uint32_t>(pids.size());
  for (const auto pid : pids) {
    const auto tr = core::win::trim_working_set_by_pid(pid);
    if (tr == core::win::TrimResult::Ok) r.succeeded++;
    else if (tr == core::win::TrimResult::AccessDenied) r.access_denied++;
    else r.failed++;
  }
  return r;
}

}  // namespace services

