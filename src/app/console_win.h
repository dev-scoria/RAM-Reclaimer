// SPDX-License-Identifier: MIT
// Copyright (c) Scoria Developers Portal. See LICENSE.

#pragma once

namespace ramc::console {

// Enables VT output where available and applies a dark console palette when Windows uses dark app mode,
// or when the environment variable RAMC_CONSOLE_DARK is set (any value).
void init();

}  // namespace ramc::console
