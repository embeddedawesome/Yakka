cmake_minimum_required(VERSION 3.20)
project(perf_test)
add_library(perf_test OBJECT ${SDK_ROOT}/./perf_test/perf_test.cpp )
