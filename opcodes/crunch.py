import sys
import os
import re
import json
from dataclasses import dataclass

@dataclass
class Field:
    hi:		int
    lo:		int
    func:	str
fields = {}

@dataclass
class Format:
    bitpat:	str             # encoding bit pattern
    reglist:	any		# rd,rs1,rs2,rs3 with type
    flags:	str
    asm:	str
formats = {}

@dataclass
class Instruction:
    bitpat:	str             # encoding bit pattern
    reglist:	any		# rd,rs1,rs2,rs3 with type
    flags:	str             # attributes and isa
    asm:	str             # how to print
    action:	str             # C++ statement for interpreter
    original:	str		# Line from .def, for debugging
    lineno:	int
    operands:	any  = None	# list of fields in instruction
    length:	int  = None	# instruction length in bytes
    immed:	str  = None	# extraction function number
    longimm:	bool = None	# if >15 bits
    code:	str  = None	# for decoder.h
    mask:	str  = None
insns = {}

@dataclass
class Hook:
    hi:		int
    lo:		int
    table:	any
hooks = {}

opcodes = [ 'ZERO' ]
mapping = {}
default_reglist = []

reg_extractors = {}
imm_extractors = {}

field_declaration	= re.compile(r'^@field\s+(\(([^)]*)\))?\s*"""')


field_line  = re.compile(r"\s*(\w+)\[(\d+):(\d+)\]\s+\'([^\']*)\'")
format_line = re.compile(r'^\s*(\w+)\s+\"([^"]*)\"\s+(\S+)\s+(\S+)\s+(\S+)')
map_section = re.compile(r'^@map\s+(\w+)\[(\d+):(\d+)\]\s+"""')
op_explicit = re.compile(r'^\s*(\S+)\s+\"([^"]*)\"\s+(\S+)\s+(\S+)\s+(\S+)\s+\"([^"]*)\"')
op_with_fmt = re.compile(r'^\s*(\S+)\s+(\S+)\s+\"([^"]*)\"(\s+\"([^"]*)\")?')

hook_spec = re.compile(r'^\?(\w+)\[(\d+)\]')

def eprint(*args):
    sys.stderr.write(' '.join(args) + '\n')

def die(*args):
    sys.stderr.write(' '.join(args) + '\n')
    exit(-1)

def unrecognized(line):
    if not re.match("^[ ]*(#.*)?", line):
        print('Unrecognized line:')
        print(line)
        exit(-1)

        
#
# Parse isa.def file
#
isa_file_name = sys.argv[1]
with open(isa_file_name) as f:
    lines = f.readlines()
    lineno = 0
    while lineno < len(lines):
        line=lines[lineno] ; lineno+=1
        if re.match(r'\s*(\#.*)$', line):
            continue
        m = field_declaration.match(line)
        if m:
            dummy, default_regs = m.groups()
            if default_regs:
                default_reglist = default_regs.split(',')
            line=lines[lineno] ; lineno+=1
            while line != '"""\n':
                m = field_line.match(line)
                if m:
                    name, hi, lo, regfunc = m.groups()
                    hi = int(hi)
                    lo = int(lo)
                    regfunc = 'int n=x({:d},{:d}); n={:s};'.format(lo, hi-lo+1, regfunc)
                    fields[name] = Field(hi, lo, regfunc)
                else:
                    unrecognized(line)
                line=lines[lineno] ; lineno+=1
        elif re.match('^@format', line):
            line=lines[lineno] ; lineno+=1
            while line != '"""\n':
                m = format_line.match(line)
                if m:
                    name, bitpat, reglist, flags, asm = m.groups()
                    flags = flags.split(',')
                    reglist = reglist.split(',')
                    formats[name] = Format(bitpat, reglist, flags, asm)
                else:
                    unrecognized(line)
                line=lines[lineno] ; lineno+=1
            continue
        
        elif re.match('^@map', line):
            m = map_section.match(line)
            if not m:
                die('Bad map declaration: ', line)
            name, hi, lo = m.groups()
            hi = int(hi)
            lo = int(lo)
            if lo > hi:
                die('Bad bit positions', line)
            table = {}
            code = 0
            line=lines[lineno] ; lineno+=1
            while line != '"""\n':
                tuple = line.split()
                for opcode in tuple:
                    if opcode[0] == '#':
                        break
                    span = 0
                    if '/' in opcode:
                        opcode, span = opcode.split('/')
                        span = int(span)
                    if opcode != '.':
                        if opcode in mapping:
                            die('Duplicate opcode mapping', opcode)
                        table[opcode] = code
                        mapping[opcode] = name
                    code += 1 << span
                line=lines[lineno] ; lineno+=1
            hooks[name] = Hook(hi, lo, table)
            continue    

        else:
            m = op_with_fmt.match(line)
            n = op_explicit.match(line)
            if m:
                name, fmt, action, dummy, description = m.groups()
                if name in insns:
                    print(line)
                    die('Duplicate opcode')
                tuple = fmt.split(',')
                format_name = tuple[0]
                if format_name not in formats:
                    die('Undefined format', format_name, 'in', line)
                fmt = formats[format_name]
                flags = fmt.flags + tuple[1:]
                insns[name] = Instruction(fmt.bitpat, fmt.reglist, flags, fmt.asm, action, line, lineno)
                opcodes.append(name)
            elif n:
                name, bitpat, reglist, flags, asm, action = n.groups()
                reglist = reglist.split(',')
                flags = flags.split(',')
                insns[name] = Instruction(bitpat, reglist, flags, asm, action, line, lineno)
                opcodes.append(name)
            else:
                if unrecognized(line):
                    print('unrecognized', line)
                    exit(-1)
                
#
# Detailed parse of instruction bit pattern
#
for opcode in insns:
    bitpat = insns[opcode].bitpat
    code, mask, pos = 0, 0, 0
    operands = []
    longimm = False
    immed = []
    # process bit pattern chucks from right to left
    for b in reversed(bitpat.split()):
        if re.match('[01]+', b):		# explicit bits of instruction
            code |= int(b, 2) << pos
            mask |= ((1<<len(b))-1) << pos
            pos += len(b)
        elif re.match('[.]+', b):		# don't-care bits of instruction
            pos += len(b)
            
        elif b[0] == '?':			# hook for bits of instruction
            m = hook_spec.match(b)
            if not m:
                die('Bad hook', b, 'in', bitpat)
            name, width = m.groups()
            width = int(width)
            if name in hooks:			#    hook to format
                h = hooks[name]                 #      = using alternate sub-format
                if opcode not in h.table:
                    die('opcode', opcode, 'not in hook table')
                value = h.table[opcode]
            elif name in mapping:		#     hook to specific instruction
                h = hooks[mapping[name]]        #       = alternate opcode
                value = h.table[name]
            else:
                print(mapping)
                die('opcode "{:s}" of "{:s}" have neither hook or mapping'.format(name, bitpat))
            if width != h.hi - h.lo + 1:
                die('Width mismatch', name, 'width', width, 'hi,lo', h.hi, h.lo, 'in', bitpat)
            code |= value << pos
            mask |= ((1 << width) -1) << pos
            pos += width
            
        elif re.match(r'^\w+', b):		# register field specifier
            if b not in fields:
                die('Unknown register field b', b, 'in', insns[opcode].original)
            operands.append(b)
            pos += fields[b].hi - fields[b].lo + 1
            
        elif re.match(r'\{[^}]+\}', b):
            b = b[1:-1]
            tuple = []
            for t in reversed(b.split('|')):
                extractor = 'x'
                if t[0] == '-':
                    extractor = 'xs'
                    t = t[1:]
                m = re.match(r'(\d+)(:(\d+))?', t)
                if not m:
                    die('Bad immediate specifier', t, 'in', bitpat)
                hi, colon, lo = m.groups()
                hi = int(hi)
                lo = int(lo) if colon else hi
                if lo > hi:
                    die('Bad bit positions', t, 'in', bitpat)
                if hi >= 12:
                    longimm = True
                shamt = '<<{:d}'.format(lo) if lo > 0 else ''
                immed.append('({:s}({:d},{:d}){:s})'.format(extractor, pos, hi-lo+1, shamt))
                pos += hi-lo+1
        else:
            eprint('Unknown field', b, 'in bits "', bitpat, '"')
            exit(-1)
    if pos % 8 != 0:
        die('Length {:d} not multiple of 8 bits'.format(pos), bitpat)
    if len(immed) == 0:
        immed = '0'
    else:
        immed = '|'.join(immed)
        if immed not in imm_extractors:
            n = len(imm_extractors)
            imm_extractors[immed] = n
        immed = 'ix{:d}(b)'.format(imm_extractors[immed])

    # Check consistency of register fields
    for r in insns[opcode].reglist:
        if r == '-':
            continue
        name, typ = r.split('.')
        if name.isnumeric() or name in operands:
            continue
        print('register field', name, 'does not appear in register list')
        print(insns[opcode].original)
        exit(-1)

    insns[opcode].operands = operands
    insns[opcode].length   = int(pos / 8)
    insns[opcode].immed    = immed
    insns[opcode].longimm  = longimm
    insns[opcode].code     = code
    insns[opcode].mask     = mask


#----------------------------------------------------------------------
#
# Next sections emit various cavatools files
#

#
# Copy file only if there are changes.  Prevents unnecessary makes.
#
def diffcp(fname):
    if os.path.exists(fname) and os.system('cmp -s newcode.tmp '+fname) == 0:
        os.system('rm newcode.tmp')
    else:
        os.system('mv newcode.tmp '+fname)


#
# Create array[opcode] of bitvector for ISA extension and ATTRibutes
#
defined_flags = {}

def emit_bitvec(within, prefix, varname):
    defined_flags[prefix] = {}
    f.write('const {:s}_bv_t {:s}[] = {{\n'.format(prefix, varname))
    for opcode in opcodes:
        if opcode not in insns:
            f.write('  /* {:19s} */ {:20s}\n'.format(opcode, '0,'))
            continue
        bv = []
        for a in insns[opcode].flags: # list of attributes
            if not a.isalnum():
                continue
            if within(a):
                bv.append('{:s}_{:s}'.format(prefix, a))
                defined_flags[prefix][a] = 1
        bv = '|'.join(bv)
        if len(bv) == 0:
            bv = '0'
        f.write('  /* {:19s} */ {:20s}\n'.format(opcode, bv+','))
    f.write('};\n\n')
    
#
# Create bitmask for stop before/after.  Bitnumber is opcode number.
#
def emit_mask(attrkey, varname):
    f.write('const uint64_t {:s}[] = {{\n'.format(varname))
    mask = 0
    n = 0
    for opcode in opcodes:
        if n == 64:
            f.write('  0x{:016x},\n'.format(mask))
            mask = 0
            n = 0
        if opcode in insns and attrkey in insns[opcode].flags:
            mask |= 1 << n
        n += 1
    f.write('  0x{:016x},\n'.format(mask))
    f.write('};\n\n')

#
# Return integer declaration big enough to contain bit vector
#
def assign_bitvec_values_returning_type(table):
    shamt = 0
    for t in sorted(table.keys()):
        table[t] = shamt
        shamt += 1
    if shamt < 8:
        bv = 'uint8_t'
    elif shamt < 16:
        bv = 'uint16_t'
    elif shamt < 32:
        bv = 'uint32_t'
    elif shamt < 64:
        bv = 'uint64_t'
    else:
        print('Bitvector too long')
    return bv

#
# Create constants.h
#
with open('newcode.tmp', 'w') as f:
    f.write('const char* op_name[] = {')
    n = 0
    for opcode in opcodes:
        if n % 4 == 0:
            f.write('\n  ')
        f.write('{:20s}'.format('"' + opcode.replace('.','_') + '",'))
        n += 1
    f.write('\n};\n\n')

    emit_bitvec(lambda a:      a.isupper() or a.isnumeric() , 'ISA', 'ISA')
    emit_bitvec(lambda a: not (a.isupper() or a.isnumeric()), 'ATTR', 'ATTR')
    emit_mask('<', 'stop_before')
    emit_mask('>', 'stop_after')
diffcp('../caveat/constants.h')


#
# Create opcodes.h
# Important:  opcode list augmented with these pseudo instructions
#
opcodes = opcodes + ['ILLEGAL', 'UNKNOWN']

with open('newcode.tmp', 'w') as f:
    f.write('enum Opcode_t : short {')
    n = 0
    for opcode in opcodes:
        if n % 4 == 0:
            f.write('\n  ')
        f.write('{:20s}'.format('Op_' + opcode.replace('.','_') + ','))
        n += 1
    f.write('\n};\n\n')
    f.write('const int Num1ber_of_Opcodes = {:d};\n\n'.format(len(opcodes)))

    for flag in sorted(defined_flags.keys()):
        table = defined_flags[flag]
        typ = assign_bitvec_values_returning_type(table)
        f.write('enum {:s}_t {{'.format(flag))
        for t in sorted(table.keys()):
            f.write('\n  {:16s} = 1 << {:d},'.format(flag+'_'+t, table[t]))
        f.write('\n};\n')
        f.write('typedef {:s} {:s}_bv_t;\n\n'.format(typ, flag))
diffcp('../caveat/opcodes.h')


#
# Create semantics.h
#

def makelist(table, n, f, name):
    list = [ None ] * n
    for t in table.keys():
        list[table[t]] = t

    f.write('int (*const {:s}[])(int32_t b) = {{'.format(name))
    for k in range(len(list)):
        f.write('\n  /* {:2d} */  [](int32_t b) {{ return {:s}; }},'.format(k, list[k]))
    f.write('\n};\n\n')
    
with open('newcode.tmp', 'w') as f:
    f.write("""
/*
  Copyright (c) 2026 Peter Hsu.  All Rights Reserved.  See LICENCE file for details.
*/
#include <stdlib.h>
#include <stdint.h>
#include <stdio.h>
#include <sys/mman.h>
#include <sys/types.h>

#include "caveat.h"
#include "hart.h"
    
#define x( pos, len) ((b >> pos) & ((1 << len)-1))
#define xs(pos, len) ((int)b << (32-pos-len) >> (32-len))

inline bool match(uint32_t b, uint32_t mask, uint32_t code, Opcode_t op)
{
  return (b & mask) == code && (ISA[op] & this_isa) == ISA[op];
}

""")
    for name in fields:
        f.write('  static int {:s}(unsigned b) {{ {:s} return n; }}\n'.format(name, fields[name].func))
    f.write('\n\n')

    for func in imm_extractors:
        f.write('  static int ix{:d}(unsigned b) {{ return {:s}; }}\n'.format(imm_extractors[func], func))
    f.write('\n\n')

    f.write("""
Insn_t decoder(uintptr_t pc)
{
  int32_t b = *(int32_t*)pc;
  Insn_t i;
  
""")
    for opcode in opcodes:
        if opcode not in insns:
            continue
        i = insns[opcode]
        digits = i.length * 2
        code = '0x'+hex(i.code)[2:].zfill(digits)
        mask = '0x'+hex(i.mask)[2:].zfill(digits)
        regs = []
        insn_regs = ['rd', 'rs1', 'rs2', 'rs3']
        for k in range(4):
            if k < len(i.reglist) and i.reglist[k] != '-':
                tuple = i.reglist[k].split('.')
                if len(tuple) != 2:
                    print(opcode, i.reglist)
                    die("Bad register list format")
                name, typ = tuple
                if name.isnumeric():
                    regs.append(name)
                elif name in fields:
                    regs.append(name+'(b)')
                else:
                    print(opcode, i.reglist)
                    die("Undefined register name", name)
            else:
                regs.append('0')
        opname = 'Op_' + opcode.replace('.','_')
        f.write('#line {:d} "{:s}"\n'.format(i.lineno, isa_file_name))
        f.write('  if (match(b, {:s}, {:s}, {:15s})) '.format(mask, code, opname))
        f.write('return Insn_t({:15s}, {:d}, {:10s}, {:10s}, '.format(opname, i.length, regs[0], regs[1]))
        if i.longimm:
            f.write('{:s});\n'.format(i.immed))
        else:
            f.write('{:10s}, {:10s}, {:s});\n'.format(regs[2], regs[3], i.immed))

    f.write("""
  fprintf(stderr, "Unrecognized instruction pc=%lx, %08x\\n", pc, *(unsigned*)pc);
  return Insn_t(Op_ILLEGAL, 4, 0, 0, 0);
}
""")
            
diffcp('../caveat/decoder.cc')


#
#  Create interpreter semantics.in
#
with open('newcode.tmp', 'w') as f:
    for opcode in insns:
        i = insns[opcode]
        opname = 'Op_'+opcode.replace('.','_')
        #
        # change register specifiers to refer to actual register with type
        #
        # print(i.original.strip())
        # print('---:', i.action)
        action = i.action
        action = action.replace('f32', 'XXXXXXX') # hack to preserve f32
        for k in range(len(i.reglist)):
            if i.reglist[k] == '-':
                continue
            name, typ = i.reglist[k].split('.')
            if name.isnumeric():
                action = re.sub(r'[rf]{:d}'.format(k), 's.reg[{:s}].{:s}'.format(name, typ), action)
            elif name in fields:
                action = re.sub(r'[rf]{:d}'.format(k), 's.reg[insn.rs{:d}()].{:s}'.format(k, typ), action)
            else:
                # print(i.lineno, i.original)
                die('Unknown field', name)
            # print('--->', action)
        action = action.replace('XXXXXXX', 'f32') # hack put back f32
        statement = '    case {:20s} {:s}; s.pc+={:d}; break;'.format(opname+':', action, i.length)
        f.write('#line {:d} "{:s}"\n'.format(i.lineno, isa_file_name))
        f.write(statement+'\n')
        # print('===>', statement)

diffcp('../caveat/semantics.h')

exit(0)
