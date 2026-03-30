// SPDX-License-Identifier: MIT
// Copyright (c) Scoria Developers Portal. See LICENSE.

#include "services/cleaner.h"
#include "app/console_win.h"
#include "ramc_strings.h"

#include <windows.h>

#include <iomanip>
#include <iostream>
#include <string>

static void print_usage() {
  std::cout
      << "RAMC — working set trim utility for Windows\n"
      << ramc::kVendorAttribution << "\n"
      << "\n"
      << "Usage:\n"
      << "  ramc status\n"
      << "  ramc clean [--all]\n"
      << "\n"
      << "Notes:\n"
      << "  - \"clean\" trims working sets. It does NOT magically increase physical RAM.\n"
      << "  - Use an elevated terminal for best results when trimming other processes.\n";
}

static void print_status(const services::MemoryStatus& st) {
  auto to_gib = [](std::uint64_t bytes) -> double {
    return static_cast<double>(bytes) / (1024.0 * 1024.0 * 1024.0);
  };

  std::cout << std::fixed << std::setprecision(2);
  std::cout << "Physical RAM: " << to_gib(st.total_phys_bytes) << " GiB total, "
            << to_gib(st.avail_phys_bytes) << " GiB available\n";
  std::cout << "Pagefile:     " << to_gib(st.total_pagefile_bytes) << " GiB total, "
            << to_gib(st.avail_pagefile_bytes) << " GiB available\n";
  std::cout << "Memory load:  " << st.memory_load_percent << "%\n";
}

int main(int argc, char** argv) {
  ramc::console::init();

  try {
    if (argc < 2) {
      print_usage();
      return 2;
    }

    const std::string cmd = argv[1];

    if (cmd == "help" || cmd == "-h" || cmd == "--help") {
      print_usage();
      return 0;
    }

    if (cmd == "status") {
      const auto st = services::get_memory_status();
      print_status(st);
      return 0;
    }

    if (cmd == "clean") {
      bool include_all = false;
      if (argc >= 3) {
        const std::string arg = argv[2];
        if (arg == "--all") include_all = true;
      }

      const auto before = services::get_memory_status();
      std::cout << "Before:\n";
      print_status(before);

      const auto res = services::clean_working_sets({.include_all_processes = include_all});

      std::cout << "\nTrimmed working sets:\n"
                << "  attempted: " << res.attempted << "\n"
                << "  succeeded: " << res.succeeded << "\n"
                << "  access denied: " << res.access_denied << "\n"
                << "  failed: " << res.failed << "\n";

      const auto after = services::get_memory_status();
      std::cout << "\nAfter:\n";
      print_status(after);

      return (res.succeeded > 0) ? 0 : 1;
    }

    std::cerr << "Unknown command: " << cmd << "\n\n";
    print_usage();
    return 2;
  } catch (const std::exception& e) {
    std::cerr << "Error: " << e.what() << "\n";
    return 1;
  }
}

