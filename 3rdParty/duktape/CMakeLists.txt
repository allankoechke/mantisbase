cmake_minimum_required(VERSION 3.22)
project(duktape VERSION 2.7.0)
add_library(duktape duk_config.h duktape.c duktape.h)