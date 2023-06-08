cmake_minimum_required(VERSION 3.10)

project(xviewmap)

find_package(X11)

set(lib_src
  src/core.cpp
  src/toml.cpp
)
set(main_src
  ${lib_src}
  src/main.cpp
)

if(${CMAKE_SOURCE_DIR} STREQUAL ${CMAKE_CURRENT_LIST_DIR})
  add_executable(xviewmap ${main_src})
  install(TARGETS xviewmap
    RUNTIME DESTINATION $ENV{HOME}/.robotech/bin
  )
else()
  add_library(xviewmap STATIC ${lib_src})
  add_library(xviewmap::xviewmap ALIAS xviewmap)
endif()

target_compile_features(xviewmap PUBLIC cxx_std_17)
target_include_directories(xviewmap PUBLIC ${CMAKE_CURRENT_LIST_DIR}/include)
target_include_directories(xviewmap PRIVATE ${X11_INCLUDE_DIR})
target_link_libraries(xviewmap PRIVATE ${X11_LIBRARIES})
