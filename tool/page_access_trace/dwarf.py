''' DWARF debugging information read from the binary. '''

import bisect
from elftools.elf.elffile import ELFFile
from elftools.dwarf.descriptions import describe_form_class

class DwarfInfo:
    ''' Encapsulate and provide APIs to query DWARF debugging information
        available in the binary.
    '''
    class AddressRange:
        ''' An address range in the binary which corresponds to a specific
            source code location.
        '''
        def __init__(self, address, size, filename, linenum):
            self.address = address
            self.size = size
            self.endRange = address + size
            self.filename = filename
            self.linenum = linenum

        def contains(self, address):
            return self.address <= address < self.endRange

        def __lt__(self, other): return self.address < other.address

        def __str__(self):
            return "{:x} - {:x}: {}:{}".format(self.address,
                self.address + self.size, self.filename, self.linenum)

    def __init__(self, filename):
        self.filename = filename
        with open(filename, 'rb') as fp:
            elf = ELFFile(fp)
            assert elf.has_dwarf_info(), \
                   "No DWARF information for '{}'".format(filename)
            self.parseAddressRanges(elf.get_dwarf_info())

    def parseAddressRanges(self, dwarfinfo):
        self.addrRanges = []
        for CU in dwarfinfo.iter_CUs():
            lineprog = dwarfinfo.line_program_for_CU(CU)
            prevstate = None
            for entry in lineprog.get_entries():
                if entry.state is None or entry.state.end_sequence: continue
                elif prevstate:
                    size = entry.state.address - prevstate.address
                    filename = lineprog['file_entry'][prevstate.file - 1].name
                    line = prevstate.line
                    self.addrRanges.append(
                        DwarfInfo.AddressRange(prevstate.address,
                                               size,
                                               filename.decode("utf-8"),
                                               line))
                prevstate = entry.state
        self.addrRanges.sort()
        self.addresses = [ ar.address for ar in self.addrRanges ]

    def getFileAndLine(self, address):
        ''' Get the file & line number for the faulting address. '''
        idx = bisect.bisect_right(self.addresses, address)
        if idx != 0 and self.addrRanges[idx-1].contains(address):
            return self.addrRanges[idx-1].filename, \
                   self.addrRanges[idx-1].linenum
        else: return None, 0

