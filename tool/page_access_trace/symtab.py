''' Symbol table read from the binary.  Encapsulates much of the information
    described in the ELF specification.
'''

import bisect
from elftools.elf.elffile import ELFFile
from elftools.elf.sections import SymbolTableSection

class Symbol:
    ''' A symbol read from the binary's symbol table.  Translates ELF-specific
        information into a query-able API.
    '''

    ''' Symbol types as defined in the ELF specification '''
    NoType=0
    ObjectType=1 # I.e., static data
    FunctionType=0 # I.e., code
    SectionType=3
    FileType=4
    CommonType=5
    TLSType=6

    @classmethod
    def typeNameToInt(cls, name):
        ''' Convert an ELF symbol type to an integer '''
        if name == "STT_OBJECT": return cls.ObjectType
        elif name == "STT_FUNC": return cls.FunctionType
        elif name == "STT_SECTION": return cls.SectionType
        elif name == "STT_FILE": return cls.FileType
        elif name == "STT_COMMON": return cls.CommonType
        elif name == "STT_TLS": return cls.TLSType
        else: return cls.NoType

    ''' Symbol type getters. '''
    def isObject(self): return self.symtype == self.ObjectType
    def isFunction(self): return self.symtype == self.FunctionType
    def isSection(self): return self.symtype == self.SectionType
    def isFile(self): return self.symtype == self.FileType
    def isCommon(self): return self.symtype == self.CommonType
    def isTLS(self): return self.symtype == self.TLSType

    def isData(self):
        ''' Return whether the symbol is a data object. '''
        return self.isObject() or self.isCommon()

    def isCode(self):
        ''' Return whether the symbol is a code object. '''
        return self.isFunction()

    ''' Symbol binding type, i.e., symbol visibility '''
    LocalBind=0
    GlobalBind=1
    WeakBind=2

    @classmethod
    def bindNameToInt(cls, name):
        ''' Convert an ELF symbol bind type to an integer '''
        if name == "STB_LOCAL": return cls.LocalBind
        elif name == "STB_GLOBAL": return cls.GlobalBind
        elif name == "STB_WEAK": return cls.WeakBind
        else: return -1

    ''' Binding type getters. '''
    def isLocal(self): return self.bindtype == self.LocalBind
    def isGlobal(self): return self.bindtype == self.GlobalBind
    def isWeak(self): return self.bindtype == self.WeakBind

    def contains(self, addr):
        ''' Return whether the symbol contains a given address. '''
        if self.addr <= addr and addr < (self.addr + self.size): return True
        else: return False

    def __init__(self, name, addr, size, info):
        ''' Create a symbol using values read from the ELF symbol table. '''
        self.name = name
        self.addr = addr
        self.size = size
        self.symtype = Symbol.typeNameToInt(info["type"])
        self.bindtype = Symbol.bindNameToInt(info["bind"])

    def __lt__(self, other): return self.addr < other.addr

class SymbolTable:
    ''' A symbol table.  Duh. '''

    def __init__(self, binary):
        # TODO be verbose if requested
        ''' Read the symbol table from a file.  Return true if we parsed it
            correctly, false otherwise.
        '''
        with open(binary, 'rb') as binfp:
            elf = ELFFile(binfp)
            symtabSection = elf.get_section_by_name(".symtab")
            assert symtabSection, "No symbol table"

            self.symbols = {}
            for sym in symtabSection.iter_symbols():
                self.symbols[sym.name] = Symbol(sym.name,
                                                sym["st_value"],
                                                sym["st_size"],
                                                sym["st_info"])

            # Symbols occupy a range of memory rather than a single address,
            # making symbol lookups based on a faulting address complicated.
            # Create a list of symbols sorted by starting address to facilitate
            # a hybrid binary search + range check approach for symbol lookup.
            self.sortedsyms = list(self.symbols.values())
            self.sortedsyms.sort(key=lambda sym: sym.addr)

    def getSymbol(self, addr):
        ''' Look up the symbol containing the faulting address.

            Because symbols occupy a range of memory, we can't simply use a
            hash table to map addresses to symbols.  Instead, we need an
            ordering, so that we can determine the symbol whose starting
            address is directly before or equal to the faulting address.  Once
            we find this symbol (if it exists), we can do a simple range check
            to see if the symbol contains the faulting address.

            Returns the Symbol object corresponding to the faulting address, or
            None if the address doesn't correspond to any symbol.
        '''
        tmpSymbol = Symbol(None, addr, 0, {"type" : "", "bind" : ""})
        idx = bisect.bisect_left(self.sortedsyms, tmpSymbol)
        if idx != len(self.sortedsyms) and self.sortedsyms[idx].contains(addr):
            return self.sortedsyms[idx]
        else: return None

