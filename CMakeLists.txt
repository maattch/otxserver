cmake_minimum_required(VERSION 3.19)

project(otx CXX)

set(CMAKE_DISABLE_SOURCE_CHANGES ON)
set(CMAKE_DISABLE_IN_SOURCE_BUILD ON)

set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_STANDARD_REQUIRED ON)

list(APPEND CMAKE_MODULE_PATH "${CMAKE_CURRENT_SOURCE_DIR}/cmake")

if (NOT WIN32)
    add_compile_options(-Wall -Wextra -Wnon-virtual-dtor -Wold-style-cast -pedantic -pipe -fvisibility=hidden)
endif ()

if ("${CMAKE_CXX_COMPILER_ID}" STREQUAL "GNU")
    add_compile_options(-fno-strict-aliasing)
endif ()

if ("${CMAKE_CXX_COMPILER_ID}" STREQUAL "Clang")
    add_compile_options(-Wimplicit-fallthrough -Wmove)
endif ()

add_compile_options("$<$<CXX_COMPILER_ID:MSVC>:/utf-8>")

# Find packages.
find_package(Threads REQUIRED)

find_package(OpenSSL 3.0.0 REQUIRED COMPONENTS Crypto)
include_directories(${OPENSSL_INCLUDE_DIRS})

find_package(Boost 1.71.0 REQUIRED COMPONENTS system)
include_directories(${Boost_INCLUDE_DIRS})

find_package(LuaJIT REQUIRED)
include_directories(${LUA_INCLUDE_DIR})

find_package(MySQL REQUIRED)
include_directories(${MYSQL_INCLUDE_DIRS})

find_package(LibXml2 REQUIRED)
include_directories(${LIBXML2_INCLUDE_DIR})

find_package(GMP REQUIRED)
include_directories(${GMP_INCLUDE_DIR})

# Option to disable unity builds
option(ENABLE_UNITY_BUILD "Enable unity build" ON)

add_subdirectory(src)
add_executable(otx ${otx_MAIN})
target_link_libraries(otx otx_lib)

### INTERPROCEDURAL_OPTIMIZATION ###
include(CheckIPOSupported)
check_ipo_supported(RESULT result OUTPUT error)
if (result)
    set(CMAKE_INTERPROCEDURAL_OPTIMIZATION TRUE)
else ()
    message(STATUS "IPO is not supported: ${error}")
endif ()
### END INTERPROCEDURAL_OPTIMIZATION ###

target_precompile_headers(otx PUBLIC src/otpch.h)
