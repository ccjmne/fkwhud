cmake_minimum_required(VERSION 3.10)
project(fcitx5-fkwhud)

find_package(PkgConfig REQUIRED)
pkg_check_modules(DEPS REQUIRED Fcitx5Core)

add_library(fkwhud MODULE fkwhud.cpp)
set_target_properties(fkwhud PROPERTIES PREFIX "" OUTPUT_NAME "fkwhud")
target_compile_features(fkwhud PRIVATE cxx_std_20)
target_include_directories(fkwhud PRIVATE ${DEPS_INCLUDE_DIRS})
target_link_libraries(fkwhud ${DEPS_LIBRARIES})

include(GNUInstallDirs)
install(TARGETS fkwhud        DESTINATION "${CMAKE_INSTALL_LIBDIR}/fcitx5")
install(FILES   fkwhud.conf   DESTINATION "${CMAKE_INSTALL_DATADIR}/fcitx5/addon")
