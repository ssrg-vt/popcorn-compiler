/*******************************************************************************
** HOST Code HEADER FILE
*******************************************************************************/


#pragma once

#define CL_HPP_CL_1_2_DEFAULT_BUILD
#define CL_HPP_TARGET_OPENCL_VERSION 120
#define CL_HPP_MINIMUM_OPENCL_VERSION 120
#define CL_HPP_ENABLE_PROGRAM_CONSTRUCTION_FROM_ARRAY_COMPATIBILITY 1
#define CL_USE_DEPRECATED_OPENCL_1_2_APIS

#include <fcntl.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <stdbool.h>
#include <CL/cl_ext.h>
#define MEM_ALIGNMENT 4096

#define TIMERS_ALL
#define PRINT_INFO
#define CLFINISH_INTERNAL


cl_context context;
cl_command_queue commands;
cl_program program;
cl_kernel fpga_kernel;
