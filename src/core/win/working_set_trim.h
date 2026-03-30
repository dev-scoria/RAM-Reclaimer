#pragma once

#include <cstdint>

namespace core::win {

enum class TrimResult {
  Ok,
  AccessDenied,
  Failed,
};

TrimResult trim_working_set_by_pid(std::uint32_t pid);

}  // namespace core::win

