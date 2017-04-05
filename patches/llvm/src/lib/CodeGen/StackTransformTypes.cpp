//===-- llvm/Target/TargetValueGenerator.cpp - Value Generator --*- C++ -*-===//
//
//                     The LLVM Compiler Infrastructure
//
// This file is distributed under the University of Illinois Open Source
// License. See LICENSE.TXT for details.
//
//===----------------------------------------------------------------------===//

#include "llvm/CodeGen/StackTransformTypes.h"

using namespace llvm;

const char *MachineGeneratedVal::ValueGenInst::InstTypeStr[] = {
  #define X(type) #type
  VALUE_GEN_INST
  #undef X
};

