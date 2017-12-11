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

#include <cstdint>
#include <string>
#include <vector>
#include <map>
#include <memory>
#include "llvm/CodeGen/MachineOperand.h"
#include "llvm/CodeGen/StackTransformTypes.def"
#include "llvm/ADT/SmallVector.h"

namespace llvm {

class AsmPrinter;
class Instruction;
class MachineInstr;
class MCSymbol;
class Value;

//===----------------------------------------------------------------------===//
// Types for generating more complex architecture-specific live values
//

#define INV_INST_TYPE "Invalid instruction type"

/// ValueGenInst - an instruction for the transformation runtime which generates
/// a live value.  These instructions specify a simple operation (e.g., add) and
/// an operand.  These instructions, coupled together with a destination
/// location (i.e., a register or stack slot), allow the runtime to construct
/// more complex live values like bit-shifts or pointers into arrays of structs.
class ValueGenInst {
public:
  virtual ~ValueGenInst() {}

  /// Instruction types
  enum InstType {
#define X(type) type,
    VALUE_GEN_INST
#undef X
  };
  virtual InstType type() const = 0;

  /// Operand types
  enum OpType { Register, Immediate };
  virtual OpType opType() const = 0;

  /// Equivalence checking.  Depends on both instruction & operand type, and
  /// any operand-specific information.
  virtual bool operator==(const ValueGenInst &RHS) const = 0;

  /// Get a human-readable name for the instruction type
  static const char *getInstName(enum InstType Type);
  static std::string getInstNameStr(enum InstType Type);

  /// Get a human-readable description of the instruction & operand
  virtual std::string str() const = 0;
private:
  static const char *InstTypeStr[];
};

/// RegInstructionBase - base class for register operand instructions.  The
/// register is stored as an architecture-specific physical register.
class RegInstructionBase : public ValueGenInst {
public:
  virtual OpType opType() const { return Register; }
  unsigned getReg() const { return Reg; }
  void setReg(unsigned Reg) { this->Reg = Reg; }

protected:
  /// The register used in the instruction
  // Note: will be converted to DWARF during metadata emission
  unsigned Reg;

  RegInstructionBase(unsigned Reg) : Reg(Reg) {}
};

/// RegInstruction<T> - register-based instructions.  Instructions are
/// specified via template argument.
template<ValueGenInst::InstType Type>
class RegInstruction : public RegInstructionBase {
  static_assert(Type == Set || Type == Add || Type == Subtract ||
                Type == Multiply || Type == Divide,
                INV_INST_TYPE " for register instruction");
public:
  RegInstruction(unsigned Reg) : RegInstructionBase(Reg) {}

  virtual InstType type() const { return Type; }

  virtual bool operator==(const ValueGenInst &RHS) const {
    if(RHS.type() == Type && RHS.opType() == Register) {
      const RegInstruction<Type> &RI = (const RegInstruction<Type> &)RHS;
      if(RI.Reg == Reg) return true;
    }
    return false;
  }

  virtual std::string str() const
  { return getInstNameStr(Type) + " register " + std::to_string(Reg); }
};

/// ImmInstructionBase - base class for immediate operand instructions.
class ImmInstructionBase : public ValueGenInst {
public:
  virtual OpType opType() const { return Immediate; }
  unsigned getImmSize() const { return Size; }
  int64_t getImm() const { return Imm; }
  void setImm(unsigned Size, int64_t Imm)
  { this->Size = Size; this->Imm = Imm; }

protected:
  unsigned Size; // in bytes
  int64_t Imm;

  ImmInstructionBase(unsigned Size, int64_t Imm) : Size(Size), Imm(Imm) {}
};

/// ImmInstruction<T> - rmmediate-based instructions.  Instructions are
/// specified via template argument.
template<ValueGenInst::InstType Type>
class ImmInstruction : public ImmInstructionBase {
public:
  ImmInstruction(unsigned Size, int64_t Imm) : ImmInstructionBase(Size, Imm) {}

  virtual InstType type() const { return Type; }

  virtual bool operator==(const ValueGenInst &RHS) const {
    if(RHS.type() == Type && RHS.opType() == Immediate) {
      const ImmInstruction<Type> &II = (const ImmInstruction<Type> &)RHS;
      if(II.Imm == Imm && II.Size == Size) return true;
    }
    return false;
  }

  virtual std::string str() const
  { return getInstNameStr(Type) + " immediate " + std::to_string(Imm); }
};

/// Wrap raw pointers to ValueGenInst in smart pointers.  Use shared_ptr so we
/// can use copy constructors for containers of these instructions.
typedef std::shared_ptr<ValueGenInst> ValueGenInstPtr;

/// A list of instructions used to generate a value
typedef std::vector<ValueGenInstPtr> ValueGenInstList;

#undef INV_INST_TYPE

//===----------------------------------------------------------------------===//
// Machine-specific live values
//
// These are the live values used to populate an architecture-specific location,
// e.g., a reference to a global symbol or an immediate value

/// MachineLiveVal - A machine-specific live value
class MachineLiveVal {
public:
  /// Constructors & destructors.
  // Note: create child class objects rather than objects of this class.
  virtual ~MachineLiveVal() {}
  virtual MachineLiveVal *copy() const = 0;

  /// Possible live value types
  enum Type { SymbolRef, ConstPoolRef, StackObject, Immediate, Generated };

  /// Determine the live value's type
  virtual enum Type getType() const = 0;
  virtual bool isReference() const { return false; }
  virtual bool isSymbolRef() const { return false; }
  virtual bool isConstPoolRef() const { return false; }
  virtual bool isStackObject() const { return false; }
  virtual bool isImm() const { return false; }
  virtual bool isGenerated() const { return false; }

  /// Equivalence checking
  virtual bool operator==(const MachineLiveVal &RHS) const = 0;

  /// Generate a human-readable string describing the value
  virtual std::string toString() const = 0;

  /// Get the machine instruction which defines the live value
  const MachineInstr *getDefiningInst() const { return DefMI; }

  /// Return whether this value is a pointer
  // Note: if the value *could* be a pointer, this should be set so the runtime
  // can do pointer-to-stack checks
  bool isPtr() const { return Ptr; }

protected:
  /// Defining instruction for live value
  const MachineInstr *DefMI;

  /// Is this a pointer?
  // Note: if the value *could* be a pointer, this should be set so the runtime
  // can do pointer-to-stack checks
  bool Ptr;

  MachineLiveVal(const MachineInstr *DefMI, bool Ptr)
    : DefMI(DefMI), Ptr(Ptr) {}
  MachineLiveVal(const MachineLiveVal &C) : DefMI(C.DefMI), Ptr(C.Ptr) {}
};

/// MachineReference - a reference to some binary object outside of thread
/// local storage
class MachineReference : public MachineLiveVal {
public:
  virtual bool isReference() const { return true; }

  /// Get a symbol reference for label generation
  virtual MCSymbol *getReference(AsmPrinter &AP) const = 0;

protected:
  MachineReference(const MachineInstr *DefMI, bool Ptr)
    : MachineLiveVal(DefMI, Ptr) {}
  MachineReference(const MachineReference &C) : MachineLiveVal(C) {}
};

/// MachineSymbolRef - a reference to a global symbol
class MachineSymbolRef : public MachineReference {
public:
  MachineSymbolRef(const MachineOperand &Symbol,
                   const MachineInstr *DefMI,
                   bool Ptr)
    : MachineReference(DefMI, Ptr), Symbol(Symbol) {}
  MachineSymbolRef(const MachineSymbolRef &C)
    : MachineReference(C), Symbol(C.Symbol) {}
  virtual MachineLiveVal *copy() const { return new MachineSymbolRef(*this); }

  virtual enum Type getType() const { return SymbolRef; }
  virtual bool isSymbolRef() const { return true; }

  virtual bool operator==(const MachineLiveVal &RHS) const;
  virtual std::string toString() const;
  virtual MCSymbol *getReference(AsmPrinter &AP) const;

private:
  // MCSymbols may not exist yet, so instead store the operand to look up the
  // MCSymbol at metadata emission time.
  // Note: store hard-copy (not reference) because optimizations may convert
  // symbol reference to a different type, e.g., register
  const MachineOperand Symbol;
};

/// MachineConstPoolRef - a reference to a constant pool entry
class MachineConstPoolRef : public MachineReference {
public:
  MachineConstPoolRef(int Index, const MachineInstr *DefMI, bool Ptr = false)
    : MachineReference(DefMI, Ptr), Index(Index) {}
  MachineConstPoolRef(const MachineConstPoolRef &C)
    : MachineReference(C), Index(C.Index) {}
  virtual MachineLiveVal *copy() const
  { return new MachineConstPoolRef(*this); }

  virtual enum Type getType() const { return ConstPoolRef; }
  virtual bool isConstPoolRef() const { return true; }

  virtual bool operator==(const MachineLiveVal &RHS) const;

  virtual std::string toString() const
  { return "reference to constant pool index " + std::to_string(Index); }

  virtual MCSymbol *getReference(AsmPrinter &AP) const;

private:
  int Index;
};

/// MachineStackObject - an object on the stack
class MachineStackObject : public MachineLiveVal {
public:
  MachineStackObject(int Index,
                     bool Load,
                     const MachineInstr *DefMI,
                     bool Ptr = false)
    : MachineLiveVal(DefMI, Ptr), Index(Index), Load(Load) {}
  MachineStackObject(const MachineStackObject &C)
    : MachineLiveVal(C), Index(C.Index), Load(C.Load) {}
  virtual MachineLiveVal *copy() const
  { return new MachineStackObject(*this); }

  /// Objects common across stack frames for all supported architectures
  enum Common { None, ReturnAddr = INT32_MAX };

  virtual enum Type getType() const { return StackObject; }
  virtual bool isStackObject() const { return true; }
  virtual enum Common getCommonObjectType() const { return None; }
  virtual bool isCommonObject() const { return false; }

  virtual bool operator==(const MachineLiveVal &RHS) const;

  virtual std::string toString() const;

  /// Return the object's offset from a base register (returned in BR)
  virtual int getOffsetFromReg(AsmPrinter &AP, unsigned &BR) const;

  int getIndex() const { return Index; }
  void setIndex(int Index) { this->Index = Index; }
  bool isLoad() const { return Load; }
  void setLoad(bool Load) { this->Load = Load; }

private:
  /// The stack slot index of a stack object
  int Index;

  /// Are we generating a reference to a stack object or loading a value from
  /// the stack slot?
  bool Load;
};

/// ReturnAddress - the return address stored on the stack
class ReturnAddress : public MachineStackObject {
public:
  ReturnAddress(const MachineInstr *DefMI)
    : MachineStackObject(ReturnAddr, true, DefMI, false) {}
  ReturnAddress(const ReturnAddress &C) : MachineStackObject(C) {}
  virtual MachineLiveVal *copy() const { return new ReturnAddress(*this); }

  virtual enum Common getCommonObjectType() const { return ReturnAddr; }
  virtual bool isCommonObject() const { return true; }

  virtual std::string toString() const
  { return "function return address"; }

  virtual int getOffsetFromReg(AsmPrinter &AP, unsigned &BR) const;
};

/// MachineImmediate - an immediate value
class MachineImmediate : public MachineLiveVal {
public:
  MachineImmediate(unsigned Size,
                   uint64_t Value,
                   const MachineInstr *DefMI,
                   bool Ptr = false);
  MachineImmediate(const MachineImmediate &C)
    : MachineLiveVal(C), Size(C.Size), Value(C.Value) {}
  virtual MachineLiveVal *copy() const { return new MachineImmediate(*this); }

  virtual enum Type getType() const { return Immediate; }
  virtual bool isImm() const { return true; }

  virtual bool operator==(const MachineLiveVal &RHS) const;

  virtual std::string toString() const
  { return "immediate value: " + std::to_string(Value); }

  unsigned getSize() const { return Size; }
  uint64_t getValue() const { return Value; }

private:
  unsigned Size; // in bytes
  uint64_t Value;
};

/// MachineGeneratedVal - a value generated through a set of small operations
class MachineGeneratedVal : public MachineLiveVal {
public:
  MachineGeneratedVal(const ValueGenInstList &VG,
                      const MachineInstr *DefMI,
                      bool Ptr)
    : MachineLiveVal(DefMI, Ptr), VG(VG) {}
  virtual MachineLiveVal *copy() const
  { return new MachineGeneratedVal(*this); }

  enum Type getType() const { return Generated; }
  bool isGenerated() const { return true; }

  virtual bool operator==(const MachineLiveVal &RHS) const;

  virtual std::string toString() const
  { return "generated value, " + std::to_string(VG.size()) + " instruction(s)"; }

  const ValueGenInstList &getInstructions() const { return VG; }

private:
  ValueGenInstList VG;
};

// TODO add API to generate "Operation"

//===----------------------------------------------------------------------===//
// Machine-specific locations
//
// These are locations to be populated with the live values, e.g., a register or
// stack slot.
//
// Note: these represent a live value's *destination*, not the live value
// itself.  For example, don't confuse MachineStackObject above (a live value
// to be copied from a stack slot) versus MachineLiveStackSlot below (the
// location where a live value will be stored).

/// MachineLiveLoc - an architecture-specific location for a live value
class MachineLiveLoc {
public:
  /// Constructors & destructors.
  // Note: create child class objects rather than objects of this class.
  virtual ~MachineLiveLoc() {}
  virtual MachineLiveLoc *copy() const = 0;
  virtual bool operator==(const MachineLiveLoc &R) const = 0;

  /// Determine the live value location type
  virtual bool isReg() const { return false; }
  virtual bool isStackAddr() const { return false; }
  virtual bool isStackSlot() const { return false; }

  virtual std::string toString() const = 0;
};

/// MachineLiveReg - a live value stored in a register.  Stores the register
/// number as an architecture-specific physical register.
class MachineLiveReg : public MachineLiveLoc {
public:
  MachineLiveReg(unsigned Reg) : Reg(Reg) {}
  MachineLiveReg(const MachineLiveReg &C) : Reg(C.Reg) {}
  virtual MachineLiveLoc *copy() const { return new MachineLiveReg(*this); }

  virtual bool isReg() const { return true; }

  virtual bool operator==(const MachineLiveLoc &RHS) const;

  unsigned getReg() const { return Reg; }
  void setReg(unsigned Reg) { this->Reg = Reg; }

  virtual std::string toString() const
  { return "live value in register " + std::to_string(Reg); }

private:
  unsigned Reg;
};

/// MachineLiveStackAddr - a live value stored at a known stack address.  Can
/// be used for stack objects at hard-coded offsets, e.g., the TOC pointer save
/// location for PowerPC/ELFv2 ABI.
class MachineLiveStackAddr : public MachineLiveLoc {
public:
  MachineLiveStackAddr() : Offset(INT32_MAX), Reg(UINT32_MAX), Size(0) {}
  MachineLiveStackAddr(int Offset, unsigned Reg, unsigned Size)
    : Offset(Offset), Reg(Reg), Size(Size) {}
  MachineLiveStackAddr(const MachineLiveStackAddr &C)
    : Offset(C.Offset), Reg(C.Reg), Size(C.Size) {}
  virtual MachineLiveLoc *copy() const
  { return new MachineLiveStackAddr(*this); }

  virtual bool isStackAddr() const { return true; }

  virtual bool operator==(const MachineLiveLoc &RHS) const;

  int getOffset() const { return Offset; }
  void setOffset(int Offset) { this->Offset = Offset; }
  unsigned getReg() const { return Reg; }
  void setReg(unsigned Reg) { this->Reg = Reg; }
  void setSize(unsigned Size) { this->Size = Size; }

  // Calculate the final position of the stack object.  Return the object's
  // location as an offset from a base pointer register.
  virtual int calcAndGetRegOffset(const AsmPrinter &AP, unsigned &BP)
  { BP = Reg; return Offset; }

  // The size of a stack object may need to be determined by code emission
  // metadata in child classes, hence the AsmPrinter argument
  virtual unsigned getSize(const AsmPrinter &AP) { return Size; }

  virtual std::string toString() const
  {
    return "live value at register " + std::to_string(Reg) +
           " + " + std::to_string(Offset);
  }

protected:
  // The object is referenced by an offset from a (physical) register's value.
  int Offset;
  unsigned Reg, Size;
};

/// MachineLiveStackSlot - a live value stored in a stack slot.  A more
/// abstract version of MachineLiveStackAddr, where the value is in a virtual
/// stack slot whose address won't be determined until instruction emission.
class MachineLiveStackSlot : public MachineLiveStackAddr {
public:
  MachineLiveStackSlot(int Index) : Index(Index) {}
  MachineLiveStackSlot(const MachineLiveStackSlot &C)
    : MachineLiveStackAddr(C), Index(C.Index) {}
  virtual MachineLiveLoc *copy() const
  { return new MachineLiveStackSlot(*this); }

  virtual bool isStackSlot() const { return true; }

  virtual bool operator==(const MachineLiveLoc &RHS) const;

  unsigned getStackSlot() const { return Index; }
  void setStackSlot(int Index) { this->Index = Index; }
  virtual int calcAndGetRegOffset(const AsmPrinter &AP, unsigned &BP);
  virtual unsigned getSize(const AsmPrinter &AP);

  virtual std::string toString() const
  { return "live value in stack slot " + std::to_string(Index); }

private:
  int Index;
};

/// Useful typedefs for data structures needed to store additional stack
/// transformation metadata not captured in the stackmap instructions.

/// Tidy up objects defined above into smart pointers
typedef std::unique_ptr<MachineLiveVal> MachineLiveValPtr;
typedef std::unique_ptr<MachineLiveLoc> MachineLiveLocPtr;

/// A vector of architecture-specific live value locations
// Note: we could use a set instead (because we want unique live values), but
// because we're using MachineLiveLoc pointers the set would only uniquify
// based on the pointer, not the pointed-to value.
typedef SmallVector<MachineLiveLocPtr, 4> MachineLiveLocs;

/// Map IR value to a list of architecture-specific live value locations.
/// Usually used to store duplicate locations for an IR value.
typedef std::map<const Value *, MachineLiveLocs> IRToMachineLocs;
typedef std::pair<const Value *, MachineLiveLocs> IRMachineLocPair;

/// Map an IR instruction to the metadata about its IR operands (and their
/// associated architecture-specific live values locations).
typedef std::map<const Instruction *, IRToMachineLocs> InstToOperands;
typedef std::pair<const Instruction *, IRToMachineLocs> InstOperandPair;

/// A pair to couple an architecture-specific location to the value used to
/// populate it, and a vector for storing several of them.
typedef std::pair<MachineLiveLocPtr, MachineLiveValPtr> ArchLiveValue;
typedef SmallVector<ArchLiveValue, 8> ArchLiveValues;

// Map an IR instruction to architecture-specific live values
typedef std::map<const Instruction *, ArchLiveValues> InstToArchLiveValues;
typedef std::pair<const Instruction *, ArchLiveValues> InstArchLiveValuePair;

}

#endif

