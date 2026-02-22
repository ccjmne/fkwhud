cmake_minimum_required(VERSION 3.10)
project(fkwhud C)

find_package(PkgConfig REQUIRED)
pkg_check_modules(DEPS REQUIRED gtk4 gtk4-layer-shell-0)

add_executable(fkwhud fkwhud.c)
target_include_directories(fkwhud PRIVATE ${DEPS_INCLUDE_DIRS})
target_compile_options(fkwhud PRIVATE -Os -ffunction-sections -fdata-sections -flto)
target_link_libraries(fkwhud ${DEPS_LIBRARIES})
target_link_options(fkwhud PRIVATE -Wl,--as-needed -Wl,--gc-sections -flto)
