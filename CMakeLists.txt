cmake_minimum_required(VERSION 3.1)
project(Einstein)

set(CMAKE_CXX_STANDARD 17)
set(CMAKE_EXPORT_COMPILE_COMMANDS ON)
set(CMAKE_CXX_FLAGS "${CMAKE_CXX_FLAGS} -fno-exceptions -fno-rtti")

add_subdirectory(fmt)
find_package(fmt)

include_directories("$ENV{HOME}/repos/llvm/build/deploy/include")
link_directories("$ENV{HOME}/repos/llvm/build/deploy/lib")

add_executable(einstein Einstein.cpp)
target_link_libraries(einstein
PRIVATE
  fmt-header-only
  ncurses
  pthread
  clangAST
  clangASTMatchers
  clangBasic
  clangTooling
  clangFrontend
  clangSerialization
  LLVMOption
  LLVMSupport
  )
