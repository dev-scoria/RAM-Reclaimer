cmake_minimum_required(VERSION 3.24)
project(ramc VERSION 0.1.0 LANGUAGES CXX)

set(CMAKE_CXX_STANDARD 20)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_CXX_EXTENSIONS OFF)

add_executable(ramc
  src/app/main.cpp
  src/app/console_win.cpp
  src/core/win/memory_status.cpp
  src/core/win/process_enum.cpp
  src/core/win/working_set_trim.cpp
  src/services/cleaner.cpp
)

target_include_directories(ramc PRIVATE "${CMAKE_CURRENT_SOURCE_DIR}/src")
target_compile_definitions(ramc PRIVATE UNICODE _UNICODE WIN32_LEAN_AND_MEAN NOMINMAX)

if (MINGW)
  target_link_libraries(ramc PRIVATE psapi)
else()
  target_link_libraries(ramc PRIVATE Psapi)
endif()

add_executable(ramc_gui WIN32
  src/gui/main_win32.cpp
  src/gui/localization.cpp
  src/gui/settings.cpp
  src/gui/admin.cpp
  src/core/win/memory_status.cpp
  src/core/win/process_enum.cpp
  src/core/win/working_set_trim.cpp
  src/services/cleaner.cpp
)

target_include_directories(ramc_gui PRIVATE "${CMAKE_CURRENT_SOURCE_DIR}/src")
target_compile_definitions(ramc_gui PRIVATE UNICODE _UNICODE WIN32_LEAN_AND_MEAN NOMINMAX)

if (MINGW)
  target_link_libraries(ramc_gui PRIVATE psapi comctl32)
  target_link_options(ramc_gui PRIVATE -municode)
else()
  target_link_libraries(ramc_gui PRIVATE Psapi Comctl32)
endif()

if (MSVC)
  target_compile_options(ramc_gui PRIVATE /utf-8)
endif()

