cmake_minimum_required(VERSION 3.20)
project(FolderOrganizer VERSION 2.0)

if(MSVC)
  set(CMAKE_MSVC_RUNTIME_LIBRARY "MultiThreaded$<$<CONFIG:Debug>:Debug>DLL" CACHE STRING "MSVC C runtime library" FORCE)
endif()

set(CMAKE_CXX_STANDARD 23)
set(CMAKE_CXX_STANDARD_REQUIRED ON)

# Find packages provided by vcpkg
find_package(nlohmann_json CONFIG REQUIRED)
find_package(Exiv2 CONFIG REQUIRED)
find_package(ftxui CONFIG REQUIRED)
find_package(GTest REQUIRED)

# --- COMPILER HARDENING AND TOOLING ---
function(add_project_hardening target)
    if(MSVC)
        target_compile_options(${target} PRIVATE $<$<CONFIG:Debug>:/W4 /WX> $<$<CONFIG:Release>:/W3> /EHa)
        target_compile_options(${target} PRIVATE $<$<CONFIG:Debug>:/analyze>)
        target_compile_options(${target} PRIVATE /sdl /guard:cf)
        target_link_options(${target} PRIVATE /DYNAMICBASE /NXCOMPAT /guard:cf)
    else() # Assuming GCC or Clang
        target_compile_options(${target} PRIVATE -Wall -Wextra -Wpedantic $<$<CONFIG:Debug>:-Werror>)
        target_compile_options(${target} PRIVATE $<$<CONFIG:Debug>:-g -fsanitize=address,undefined>)
        target_link_options(${target} PRIVATE $<$<CONFIG:Debug>:-fsanitize=address,undefined>)
        target_compile_options(${target} PRIVATE -fstack-protector-strong -fPIE)
        target_link_options(${target} PRIVATE -fPIE "-Wl,-z,relro,-z,now")
    endif()
endfunction()
# ----------------------------------------

# 1. Create a STATIC library with the core application logic.
add_library(organizer_lib STATIC
    IOManager.cpp
    RuleEngine.cpp
    utils.hpp
)
add_project_hardening(organizer_lib)
set_property(TARGET organizer_lib PROPERTY VS_GLOBAL_CodeAnalysisRuleSet "CppCoreCheckRules.ruleset")
target_include_directories(organizer_lib PUBLIC ${CMAKE_CURRENT_SOURCE_DIR})
target_link_libraries(organizer_lib PUBLIC
    nlohmann_json::nlohmann_json
    Exiv2::exiv2lib
)

# 2. Define the main executable.
add_executable(organizer
    main.cpp
    UI.cpp
)
add_project_hardening(organizer)
set_property(TARGET organizer PROPERTY VS_GLOBAL_CodeAnalysisRuleSet "CppCoreCheckRules.ruleset")
target_link_libraries(organizer PRIVATE
    organizer_lib
    ftxui::screen
    ftxui::dom
    ftxui::component
)
if(WIN32)
  target_link_libraries(organizer PRIVATE Shell32)
endif()

# 3. Enable testing and add the tests subdirectory.
enable_testing()
add_subdirectory(tests)