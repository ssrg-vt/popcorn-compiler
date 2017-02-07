//===------- StackTransformTypes.h - Stack Transform Types ------*- C++ -*-===//
//
//                     The LLVM Compiler Infrastructure
//
// This file is distributed under the University of Illinois Open Source
// License. See LICENSE.TXT for details.
//
//===----------------------------------------------------------------------===//

#ifndef LLVM_CODEGEN_STACKTRANFORMTYPES_H
#define LLVM_CODEGEN_STACKTRANFORMTYPES_H

#include <llvm/MC/MCSymbol.h>
#include <cstdint>
#include <map>
#include <memory>
#include <string>

namespace llvm {

/// MachineConstant - A constant live value.  Note: not to be used with LLVM's
/// MachineConstantPool machinery!
class MachineConstant {
public:
  /// Constructors & destructors.  Note: create child class objects rather than
  /// objects of this class.
  MachineConstant() = delete;
  virtual ~MachineConstant() {}
  virtual MachineConstant *copy() const = 0;
  virtual bool operator==(const MachineConstant &RHS) = 0;

  /// Possible constant types
  enum Type { None, Symbol, Immediate };

  /// Determine the constant's type
  bool isValid() const { return Type == None; }
  bool isSymbol() const { return Type == Symbol; }
  bool isImm() const { return Type == Immediate; }

  virtual std::string toString() const {
    switch(Type) {
    case None: return "none";
    case Symbol: return "symbol reference";
    case Immediate: return "immediate";
    default: return "unknown";
    }
  }

protected:
  /// Types
  enum Type Type;

  MachineConstant(enum Type Type = None) : Type(Type) {}
  MachineConstant(const MachineConstant &C) : Type(C.Type) {}
};

/// MachineSymbol - a machine constant referencing a symbol
class MachineSymbol : public MachineConstant {
public:
  MachineSymbol(const std::string &Symbol)
    : MachineConstant(MachineConstant::Symbol), Symbol(Symbol) {}
  MachineSymbol(const MachineSymbol &C)
    : MachineConstant(C), Symbol(C.Symbol) {}

  virtual MachineConstant *copy() const
  { return new MachineSymbol(*this); }

  virtual bool operator==(const MachineConstant &RHS) {
    if(RHS.isSymbol()) {
      const MachineSymbol &MSR = (const MachineSymbol &)RHS;
      if(MSR.Symbol == Symbol) return true;
    }
    return false;
  }

  const std::string &getSymbol() const { return Symbol; }
  void setSymbol(const std::string &Symbol) { this->Symbol = Symbol; }

  virtual std::string toString() const {
    std::string Str = MachineConstant::toString() + " '" + Symbol + "'";
    return Str;
  }

private:
  // Note: MCSymbols may not exist yet, so instead store symbol name and look
  // up MCSymbol to generate label at metadata emission time
  std::string Symbol;
};

/// MachineImmediate - an immediate machine constant
class MachineImmediate : public MachineConstant {
public:
  MachineImmediate(unsigned int Size = 8, uint64_t Value = UINT64_MAX)
    : MachineConstant(MachineConstant::Immediate), Size(Size), Value(Value) {}
  MachineImmediate(const MachineImmediate &C)
    : MachineConstant(C), Size(C.Size), Value(C.Value) {}

  virtual MachineConstant *copy() const
  { return new MachineImmediate(*this); }

  virtual bool operator==(const MachineConstant &RHS) {
    if(RHS.isImm()) {
      const MachineImmediate &MI = (const MachineImmediate &)RHS;
      if(MI.Size == Size && MI.Value == Value) return true;
    }
    return false;
  }

  virtual std::string toString() const {
    std::string Str = MachineConstant::toString() +
                      ", value: " + std::to_string(Value);
    return Str;
  }

  unsigned getSize() const { return Size; }
  uint64_t getValue() const { return Value; }
  void setSize(unsigned Size) { this->Size = Size; }
  void setValue(uint64_t Value) { this->Value = Value; }

private:
  unsigned Size; // in bytes
  uint64_t Value;
};

/// MachineLiveLoc - an architecture-specific location for a live value
class MachineLiveLoc {
public:
  /// Constructors & destructors.  Note: create child class objects rather than
  /// objects of this class.
  MachineLiveLoc() = delete;
  virtual ~MachineLiveLoc() {}
  virtual MachineLiveLoc *copy() const = 0;
  virtual bool operator==(const MachineLiveLoc &R) const = 0;

  /// Possible types of live value's storage location
  enum Type { None, Register, StackSlot };

  /// Determine the live value location type
  bool isValid() const { return Type == None; }
  bool isReg() const { return Type == Register; }
  bool isStackSlot() const { return Type == StackSlot; }

  virtual std::string toString() const {
    switch(Type) {
    case None: return "none";
    case Register: return "register";
    case StackSlot: return "stack slot";
    default: return "unknown";
    }
  }

protected:
  /// Types & attributes
  enum Type Type;
  bool Duplicate;
  bool ArchSpecific;

  MachineLiveLoc(enum Type Type = None,
                 bool Duplicate = false,
                 bool ArchSpecific = false)
    : Type(Type), Duplicate(Duplicate), ArchSpecific(ArchSpecific) {}
  MachineLiveLoc(const MachineLiveLoc &M)
    : Type(M.Type), Duplicate(M.Duplicate), ArchSpecific(M.ArchSpecific) {}
};

/// MachineLiveReg - a live value stored in a register.  Stores the register
/// number as an architecture-specific physical register.
class MachineLiveReg : public MachineLiveLoc {
public:
  MachineLiveReg(unsigned Reg = UINT32_MAX,
                 bool Duplicate = false,
                 bool ArchSpecific = false)
    : MachineLiveLoc(MachineLiveLoc::Register, Duplicate, ArchSpecific),
      Reg(Reg) {}
  MachineLiveReg(const MachineLiveReg &M) : MachineLiveLoc(M), Reg(M.Reg) {}

  virtual MachineLiveLoc *copy() const
  { return new MachineLiveReg(*this); }

  virtual bool operator==(const MachineLiveLoc &R) const {
    if(R.isReg()) {
      const MachineLiveReg &RLV = (const MachineLiveReg &)R;
      if(Reg == RLV.Reg) return true;
    }
    return false;
  }

  unsigned getReg() const { return Reg; }
  void setReg(unsigned Reg) { this->Reg = Reg; }

  virtual std::string toString() const {
    return MachineLiveLoc::toString() + " " + std::to_string(Reg);
  };

private:
  unsigned Reg;
};

/// MachineLiveStackSlot - a live value stored in a stack slot
class MachineLiveStackSlot : public MachineLiveLoc {
public:
  MachineLiveStackSlot(int StackSlot = INT32_MIN,
                       bool Duplicate = false,
                       bool ArchSpecific = false)
    : MachineLiveLoc(MachineLiveLoc::StackSlot, Duplicate, ArchSpecific),
      StackSlot(StackSlot) {}
  MachineLiveStackSlot(const MachineLiveStackSlot &M)
    : MachineLiveLoc(M), StackSlot(M.StackSlot) {}

  virtual MachineLiveLoc *copy() const
  { return new MachineLiveStackSlot(*this); }

  virtual bool operator==(const MachineLiveLoc &R) const {
    if(R.isStackSlot()) {
      const MachineLiveStackSlot &SLV = (const MachineLiveStackSlot &)R;
      if(StackSlot == SLV.StackSlot) return true;
    }
    return false;
  }

  unsigned getStackSlot() const { return StackSlot; }
  void setStackSlot(int StackSlot) { this->StackSlot = StackSlot; }

  virtual std::string toString() const {
    return MachineLiveLoc::toString() + " " + std::to_string(StackSlot);
  };

private:
  int StackSlot;
};

/// Useful typedefs for data structures needed to store additional stack
/// transformation metadata not captured in the stackmap instructions.

// Tidy up objects defined above into smart pointers
typedef std::unique_ptr<MachineConstant> MachineConstantPtr;
typedef std::unique_ptr<MachineLiveLoc> MachineLiveLocPtr;

// A wrapper & vector of architecture-specific live value locations
// Note: we could use a set instead (because we want unique live values), but
// because we're using MachineLiveLoc pointers the set would only uniquify
// based on the pointer, not the pointed-to value.
typedef SmallVector<MachineLiveLocPtr, 4> MachineLiveLocs;

// Map IR value to a list of architecture-specific live value locations.
// Usually used to store duplicate locations for an IR value.
typedef std::map<const Value *, MachineLiveLocs> IRToMachineLocs;
typedef std::pair<const Value *, MachineLiveLocs> IRMachineLocPair;

// Map an IR instruction to the metadata about its IR operands (and their
// associated architecture-specific live values locations).
typedef std::map<const Instruction *, IRToMachineLocs> InstToOperands;
typedef std::pair<const Instruction *, IRToMachineLocs> InstOperandPair;

// Architecture-specific live values are more complicated because we have to
// store the live value in addition to the location.  A pair and vector for
// encapsulating instances of architecture-specific live values.
typedef std::pair<MachineLiveLocPtr, MachineConstantPtr> ArchLiveValue;
typedef SmallVector<ArchLiveValue, 8> ArchLiveValues;

// Map an IR instruction to architecture-specific live values
typedef std::map<const Instruction *, ArchLiveValues> InstToArchLiveValues;
typedef std::pair<const Instruction *, ArchLiveValues> InstArchLiveValuePair;

}

#endif
