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
#include <map>
#include <memory>
#include <string>
#include "llvm/CodeGen/MachineInstr.h"
#include "llvm/CodeGen/StackTransformTypes.def"

namespace llvm {

//===----------------------------------------------------------------------===//
// Machine-specific live values
//

/// MachineLiveVal - A machine-specific live value
class MachineLiveVal {
public:
  /// Constructors & destructors.  Note: create child class objects rather than
  /// objects of this class.
  MachineLiveVal() = delete;
  virtual ~MachineLiveVal() {}
  virtual MachineLiveVal *copy() const = 0;
  virtual bool operator==(const MachineLiveVal &RHS) const = 0;

  /// Possible value types
  enum Type { None, Reference, Immediate, Generated };

  /// Determine the value's type
  enum Type getType() const { return Type; }
  bool isValid() const { return Type == None; }
  bool isReference() const { return Type == Reference; }
  bool isImm() const { return Type == Immediate; }
  bool isGenerated() const { return Type == Generated; }

  /// Generate a human-readable string describing the value
  virtual std::string toString() const {
    switch(Type) {
    case None: return "none";
    case Reference: return "symbol reference";
    case Immediate: return "immediate";
    case Generated: return "generated value";
    default: return "unknown";
    }
  }

  /// Getters/setters for fields
  const MachineInstr *getDefiningInst() const { return DefMI; }
  void setDefiningInst(const MachineInstr *DefMI) { this->DefMI = DefMI; }

protected:
  const enum Type Type;
  const MachineInstr *DefMI;

  MachineLiveVal(enum Type Type = None, const MachineInstr *DefMI = nullptr)
    : Type(Type), DefMI(DefMI) {}
  MachineLiveVal(const MachineLiveVal &C) : Type(C.Type), DefMI(C.DefMI) {}
};

/// MachineReference - a reference to a symbol
class MachineReference : public MachineLiveVal {
public:
  MachineReference(const std::string &Symbol, const MachineInstr *DefMI)
    : MachineLiveVal(Reference, DefMI), Symbol(Symbol) {}
  MachineReference(const MachineReference &C)
    : MachineLiveVal(C), Symbol(C.Symbol) {}

  virtual MachineLiveVal *copy() const
  { return new MachineReference(*this); }

  virtual bool operator==(const MachineLiveVal &RHS) const {
    if(RHS.isReference()) {
      const MachineReference &MR = (const MachineReference &)RHS;
      if(MR.Symbol == Symbol) return true;
    }
    return false;
  }

  const std::string &getSymbol() const { return Symbol; }
  void setSymbol(const std::string &Symbol) { this->Symbol = Symbol; }

  virtual std::string toString() const {
    std::string Str = MachineLiveVal::toString() + " '" + Symbol + "'";
    return Str;
  }

private:
  // Note: MCSymbols may not exist yet, so instead store symbol name and look
  // up MCSymbol to generate label at metadata emission time
  std::string Symbol;
};

/// MachineImmediate - an immediate value
class MachineImmediate : public MachineLiveVal {
public:
  MachineImmediate(unsigned int Size, uint64_t Value,
                   const MachineInstr *DefMI)
    : MachineLiveVal(Immediate, DefMI), Size(Size), Value(Value) {}
  MachineImmediate(const MachineImmediate &C)
    : MachineLiveVal(C), Size(C.Size), Value(C.Value) {}

  virtual MachineLiveVal *copy() const
  { return new MachineImmediate(*this); }

  virtual bool operator==(const MachineLiveVal &RHS) const {
    if(RHS.isImm()) {
      const MachineImmediate &MI = (const MachineImmediate &)RHS;
      if(MI.Size == Size && MI.Value == Value) return true;
    }
    return false;
  }

  virtual std::string toString() const {
    std::string Str = MachineLiveVal::toString() +
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

#define INV_INST_TYPE "Invalid instruction type"

/// MachineGeneratedVal - a value generated through a set of small operations
class MachineGeneratedVal : public MachineLiveVal {
public:
  class ValueGenInst {
  public:
    // Instruction mnemonics or types
    enum InstType {
      #define X(type, pseudo) type
      VALUE_GEN_INST
      #undef X
    };

    // Human-readable instruction names
    static const char *InstTypeStr[];

    // Is the instruction a pseudo-instruction?  Access using InstType values.
    static const bool PseudoInst[];

    // Operand types
    enum OpType { Register, Immediate, None };

    virtual ~ValueGenInst() {}
    virtual OpType opType() const = 0;
    virtual InstType type() const = 0;
    virtual std::string str() const = 0;
    virtual bool operator==(const ValueGenInst &RHS) const = 0;
  };

  // Wrap raw pointers to ValueGenInst in smart pointers.  Use shared_ptr so we
  // can use copy constructors for containers of these instructions.
  typedef std::shared_ptr<ValueGenInst> ValueGenInstPtr;

  // Base class for register operand instructions
  class RegInstructionBase : public ValueGenInst {
  public:
    // The register used in the instruction
    // Note: will be converted to DWARF during metadata emission
    unsigned Reg;

    RegInstructionBase(unsigned Reg) : Reg(Reg) {}
    virtual OpType opType() const { return Register; }
    unsigned getReg() const { return Reg; }
    void setReg(unsigned Reg) { this->Reg = Reg; }
  };

  // Register-based instructions
  template<ValueGenInst::InstType Type>
  class RegInstruction : public RegInstructionBase {
    static_assert(Type == Set || Type == Add || Type == Subtract ||
                  Type == Multiply || Type == Divide,
                  INV_INST_TYPE " for register instruction");
  public:
    RegInstruction(unsigned Reg) : RegInstructionBase(Reg) {}
    virtual InstType type() const { return Type; }
    virtual std::string str() const {
      std::string buf = std::string(InstTypeStr[Type]) + " register " +
                        std::to_string(Reg);
      return buf;
    }

    virtual bool operator==(const ValueGenInst &RHS) const {
      if(RHS.type() == Type && RHS.opType() == Register) {
        const RegInstruction<Type> &RI = (const RegInstruction<Type> &)RHS;
        if(RI.Reg == Reg) return true;
      }
      return false;
    }
  };

  // Base class for immediate operand instructions
  class ImmInstructionBase : public ValueGenInst {
  public:
    // The immediate value used in the instruction & its size
    unsigned Size; // in bytes
    int64_t Imm;

    ImmInstructionBase(unsigned Size, int64_t Imm) : Size(Size), Imm(Imm) {}
    virtual OpType opType() const { return Immediate; }
    unsigned getImmSize() const { return Size; }
    int64_t getImm() const { return Imm; }
    void setImm(unsigned Size, int64_t Imm)
    { this->Size = Size; this->Imm = Imm; }
  };

  // Immediate-based instructions
  template<ValueGenInst::InstType Type>
  class ImmInstruction : public ImmInstructionBase {
    static_assert(Type != StackSlot,
                  INV_INST_TYPE " for immediate instruction");
  public:
    ImmInstruction(unsigned Size, int64_t Imm)
      : ImmInstructionBase(Size, Imm) {}
    virtual InstType type() const { return Type; }
    virtual std::string str() const {
      std::string buf = std::string(InstTypeStr[Type]) + " immediate " +
                        std::to_string(Imm);
      return buf;
    }

    virtual bool operator==(const ValueGenInst &RHS) const {
      if(RHS.type() == Type && RHS.opType() == Immediate) {
        const ImmInstruction<Type> &II = (const ImmInstruction<Type> &)RHS;
        if(II.Imm == Imm && II.Size == Size) return true;
      }
      return false;
    }
  };

  // Base class for pseudo instructions
  class PseudoInstructionBase : public ValueGenInst {
  public:
    // Whatever data is needed and the operation to be implemented after
    // rewriting the pseudo-instruction
    uint64_t Data;
    InstType GenType;

    PseudoInstructionBase(uint64_t Data, InstType GenType)
      : Data(Data), GenType(GenType) {}
    virtual OpType opType() const { return None; }
    uint64_t getData() const { return Data; }
    InstType getGenType() const { return GenType; }
    void setData(uint64_t Data) { this->Data = Data; }
    void setGenType(InstType GenType) { this->GenType = GenType; }
  };

  // Pseudo-instructions which must be converted to normal instructions later
  template<ValueGenInst::InstType Type>
  class PseudoInstruction : public PseudoInstructionBase {
    static_assert(Type == StackSlot || Type == ConstantPool,
                  INV_INST_TYPE " for pseudo-instruction");
  public:
    PseudoInstruction(uint64_t Data, InstType GenType)
      : PseudoInstructionBase(Data, GenType) {}
    virtual InstType type() const { return Type; }
    virtual std::string str() const {
      std::string buf = std::string(InstTypeStr[Type]) + " (" +
                        std::to_string(Data) + ")";
      return buf;
    }

    virtual bool operator==(const ValueGenInst &RHS) const {
      if(RHS.type() == Type) {
        const PseudoInstruction<Type> &PI =
          (const PseudoInstruction<Type> &)RHS;
        if(PI.Data == Data) return true;
      }
      return false;
    }
  };

  // A list of instructions used to generate a value
  typedef std::vector<ValueGenInstPtr> ValueGenInstList;

  MachineGeneratedVal(const ValueGenInstList &VG, const MachineInstr *DefMI)
    : MachineLiveVal(Generated, DefMI), VG(VG) {}

  virtual MachineLiveVal *copy() const
  { return new MachineGeneratedVal(*this); }

  virtual bool operator==(const MachineLiveVal &RHS) const {
    if(!RHS.isGenerated()) return false;
    const MachineGeneratedVal &MGV = (const MachineGeneratedVal &)RHS;

    if(VG.size() != MGV.VG.size()) return false;
    for(size_t i = 0, num = VG.size(); i < num; i++)
      if(VG[i] != MGV.VG[i]) return false;
    return true;
  }

  ValueGenInstList &getInstructions() { return VG; }
  const ValueGenInstList &getInstructions() const { return VG; }

  virtual std::string toString() const {
    std::string buf = MachineLiveVal::toString() + ", " +
                      std::to_string(VG.size()) + " instruction(s)";
    return buf;
  }

private:
  ValueGenInstList VG;
};

#undef INV_INST_TYPE

//===----------------------------------------------------------------------===//
// Machine-specific locations
//

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

  MachineLiveLoc(enum Type Type = None) : Type(Type){}
  MachineLiveLoc(const MachineLiveLoc &M) : Type(M.Type) {}
};

/// MachineLiveReg - a live value stored in a register.  Stores the register
/// number as an architecture-specific physical register.
class MachineLiveReg : public MachineLiveLoc {
public:
  MachineLiveReg(unsigned Reg = UINT32_MAX)
    : MachineLiveLoc(MachineLiveLoc::Register), Reg(Reg) {}
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
  MachineLiveStackSlot(int StackSlot = INT32_MIN)
    : MachineLiveLoc(MachineLiveLoc::StackSlot), StackSlot(StackSlot) {}
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
typedef std::unique_ptr<MachineLiveVal> MachineLiveValPtr;
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
typedef std::pair<MachineLiveLocPtr, MachineLiveValPtr> ArchLiveValue;
typedef SmallVector<ArchLiveValue, 8> ArchLiveValues;

// Map an IR instruction to architecture-specific live values
typedef std::map<const Instruction *, ArchLiveValues> InstToArchLiveValues;
typedef std::pair<const Instruction *, ArchLiveValues> InstArchLiveValuePair;

}

#endif

