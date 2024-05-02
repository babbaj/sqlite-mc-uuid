cmake_minimum_required(VERSION 3.28)
project(sqlite_mc_uuid)

set(CMAKE_CXX_STANDARD 20)
set(CMAKE_CXX_FLAGS "${CMAKE_CXX_FLAGS} -fPIC -Wall")

find_package(PkgConfig REQUIRED)
pkg_check_modules(SQLITE IMPORTED_TARGET sqlite3)
pkg_check_modules(CURL IMPORTED_TARGET libcurl)
pkg_check_modules(JSON IMPORTED_TARGET json-c)

add_library(sqlite_mc_uuid SHARED library.cpp)
target_include_directories(sqlite_mc_uuid PRIVATE ${SQLITE_INCLUDE_DIRS})
target_link_libraries(sqlite_mc_uuid PRIVATE PkgConfig::CURL)
target_link_libraries(sqlite_mc_uuid PRIVATE PkgConfig::JSON)

