cmake_minimum_required(VERSION 3.20)
project(ads_shopping_with_coupons LANGUAGES CXX)

set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_STANDARD_REQUIRED ON)

add_executable(coupons
    code/src/main.cpp
    code/src/solvers.cpp
)
target_include_directories(coupons PRIVATE code/src)

# 固定 -O2：报告里的运行时间都在这个优化级别下测得，换优化级别数字不可比。
target_compile_options(coupons PRIVATE -O2 -Wall -Wextra)
