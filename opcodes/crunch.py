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

opcodes = []
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
with open(sys.argv[1]) as f:
    lines = f.readlines()
    while lines:
        line = lines.pop(0)
        m = field_declaration.match(line)
        if m:
            dummy, default_regs = m.groups()
            if default_regs:
                default_reglist = default_regs.split(',')
            line = lines.pop(0)
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
                line = lines.pop(0)
        elif re.match('^@format', line):
            line = lines.pop(0)
            while line != '"""\n':
                m = format_line.match(line)
                if m:
                    name, bitpat, reglist, flags, asm = m.groups()
                    flags = flags.split(',')
                    reglist = reglist.split(',')
                    formats[name] = Format(bitpat, reglist, flags, asm)
                else:
                    unrecognized(line)
                line = lines.pop(0)
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
            line = lines.pop(0)
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
                line = lines.pop(0)
            hooks[name] = Hook(hi, lo, table)
            continue    

        else:
            m = op_with_fmt.match(line)
            n = op_explicit.match(line)
            if m:
                name, fmt, action, dummy, description = m.groups()
                tuple = fmt.split(',')
                format_name = tuple[0]
                if format_name not in formats:
                    die('Undefined format', format_name, 'in', line)
                fmt = formats[format_name]
                flags = fmt.flags + tuple[1:]
                insns[name] = Instruction(fmt.bitpat, fmt.reglist, flags, fmt.asm, action)
                opcodes.append(name)
            elif n:
                name, bitpat, reglist, flags, asm, action = n.groups()
                reglist = reglist.split(',')
                flags = flags.split(',')
                insns[name] = Instruction(bitpat, reglist, flags, asm, action)
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
    signed, longimm = False, False
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
                die('Unknown register field b', b, 'in', bitpat)
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
                if hi >= 13:
                    longimm = True
                immed.append('{:s}({:d},{:d})<<{:d}'.format(extractor, pos, hi-lo+1, lo))
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
    for opcode in insns:
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
        if attrkey in insns[opcode].flags:
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

    emit_bitvec(lambda a: a.isupper(), 'ISA', 'isa_ext')
    emit_bitvec(lambda a: not a.isupper(), 'ATTR', 'attr')
    emit_mask('<', 'stop_before')
    emit_mask('>', 'stop_after')
diffcp('../caveat/constants.h')


#
# Create opcodes.h
# Important:  opcode list augmented with these pseudo instructions
#
opcodes = ['ZERO'] + opcodes + ['ILLEGAL', 'UNKNOWN']

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
    #
    #  field extraction functions
    #
    f.write("""
#define x( lo, len) ((b >> lo) & ((1 << len)-1))
#define xs(lo, len) (b << (32-lo-len) >> (32-len))

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
        opname = opcode.replace('.','_')
        i = insns[opcode]
        digits = i.length * 2
        code = '0x'+hex(i.code)[2:].zfill(digits)
        mask = '0x'+hex(i.mask)[2:].zfill(digits)
        regs = []
        for k in range(4):
            if k < len(i.reglist) and i.reglist[k] != '-':
                tuple = i.reglist[k].split('.')
                if len(tuple) < 2:
                    print(opcode, i.reglist)
                    die("Bad register list")
                regs.append('{:s}(b)'.format(tuple[0]))
            else:
                regs.append('0')
        f.write('  if (b&{:s}=={:s}) '.format(mask, code))
        f.write('return Insn_t({:15s}, {:10s}, {:10s}, '.format('Op_'+opname, regs[0], regs[1]))
        if i.longimm:
            f.write('{:s});\n'.format(i.immed))
        else:
            f.write('{:10s}, {:10s}, {:s});\n'.format(regs[2], regs[3], i.immed))
diffcp('../caveat/decoder.h')


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
        action = i.action
        for k in range(len(i.reglist)):
            rl = i.reglist[k]
            if rl != '-':
                name, typ = rl.split('.')
                into = 's.reg[r{:d}].{:s}'.format(k, typ)
                action = action.replace('r{:d}'.format(k), into)
        f.write('    case {:20s} {:s}; s.pc+={:d}; break;\n'.format(opname+':', action, i.length))
diffcp('../caveat/semantics.h')


exit(0)

with open('newcode.tmp', 'w') as f:
    for opcode in insn_in_order:
        (opname, asm, attr, code, mask, bytes, immed, immtyp, reglist, action, regtypes) = instructions[opcode]
        if immtyp == 1:
            f.write('  if (match(b, {:s}, {:s}, {:15s}, i, {:15s}, {:15s}, {:15s})) return i;\n' \
                    .format(mask, code, opname, reglist[0], reglist[1], immed))
        else:
            f.write('  if (match(b, {:s}, {:s}, {:15s}, i, {:15s}, {:15s}, {:15s}, {:15s}, {:15s})) return i;\n' \
                    .format(mask, code, opname, reglist[0], reglist[1], reglist[2], reglist[3], immed))
diffcp('../caveat/decoder.h')

exit(0)


if insns[opcode].length not in opsizes:
    opsizes[insns[opcode].length] = []
opsizes[insns[opcode].length].append(opcode)

for opcode in opcodes:
    code = insns[opcode].code
    mask = insns[opcode].mask
    digits = insns[opcode].length * 2
    code = '0x'+hex(code)[2:].zfill(digits)
    mask = '0x'+hex(mask)[2:].zfill(digits)
    print('if (match(b,', mask, code, 'Op_'+opcode.replace('.', '_'))

exit(0)




opcode_line = re.compile(r'^([ ]|\S+)\s+(\S+)\s+(\S+)\s+(\S+)\s+\"(.*)\"\s+(\S+)\s+\"(.*)\"')
#reglist_field = re.compile(r'^(\S+)\[(\d+):(\d+)\](\+\d+)?$')

#regspecpat = re.compile(r'^([xrfd])(.*)$')
regspecpat = re.compile(r'^([bhwlufd]+)(.*)$')
regnames = {
    'c1':(9, 7, "+8"), 'c2':(4, 2, "+8"), 'c3':(6, 2, ""),
    'rd':(11, 7, ""),  's1':(19, 15, ""), 's2':(24, 20, ""), 's3':(31, 27, ""),
            }

regspec = {}
numregspecs = 0
immspec = {}
numimmspecs = 0

Last_Compressed_Opcode = 0

def diffcp(fname):
    if os.path.exists(fname) and os.system('cmp -s newcode.tmp '+fname) == 0:
        os.system('rm newcode.tmp')
    else:
        os.system('mv newcode.tmp '+fname)


compressed = []
standard = []
for filename in sys.argv[1:]:
    with open(filename) as f:
        for line in f:
            m = opcode_line.match(line)
            if not m:
                continue
            tuple = m.groups()
            if tuple[0][0] == '#':
                continue
            if tuple[1][0:2] == 'c.':
                compressed.append(tuple)
            else:
                standard.append(tuple)

instructions = {}
insn_in_order = []
for tuple in compressed+standard:
    (kind, opcode, asm, attr, bits, reglist, action) = tuple
    attr = attr.split(',')
    if '+' in kind:
        attr.append('custom')
    if '!' in kind:
        if opcode in instructions:
            #print('ignoring spike opcode', opcode)
            continue
        attr.append('spike')
    opname = 'Op_' + opcode.replace('.', '_')

    # Parse opcode
    (code, mask, pos) = (0, 0, 0)
    immed = []
    immtyp = -1
    for b in reversed(bits.split()):
        if re.match('[01]+', b):
            code |= int(b, 2) << pos
            mask |= ((1<<len(b))-1) << pos
            pos += len(b)
        elif re.match('[.]+', b):
            pos += len(b)
        elif re.match(r'\{[^}]+\}', b):
            immtyp = 0
            tuple = []
            signed = False
            i = 0
            if b[1] == '-':
                signed = True
                i = 1
            while b[i] != '}':
                i += 1
                m = re.match(r'(\d+)(:\d+)?', b[i:])
                if not m:
                    eprint('Bad immediate', name, b[i:])
                    exit(-1)
                hi = int(m.group(1))
                lo = hi
                if m.group(2):
                    lo = int(m.group(2)[1:])
                tuple.append((hi, lo))
                i += len(m.group(0))
            for (hi, lo) in reversed(tuple[1:]):
                shift = lo and '<<{:d}'.format(lo) or ''
                immed.append('x({:d},{:d}){:s}'.format(pos, hi-lo+1, shift))
                pos += hi-lo+1
            (hi, lo) = tuple[0]
            if hi >= 13:
                immtyp = 1
            shift = lo and '<<{:d}'.format(lo) or ''
            immed.append('{:s}({:d},{:d}){:s}'.format(signed and 'xs' or 'x', pos, hi-lo+1, shift))
            pos += hi-lo+1
        else:
            eprint('Unknown field', b, 'in bits "', bits, '"')
            exit(-1)
    if immed:
        immed = '|'.join(reversed(immed))
    else:
        immed = '0'
    if immed not in immspec:
        immspec[immed] = numimmspecs
        numimmspecs += 1
    immed = 'immspec[{:d}](b)'.format(immspec[immed])
    if pos == 16:
        digits = 4
    elif pos == 32:
        digits = 8
    else:
        eprint('Illegal length', pos, 'in bits "', bits, '"')
        eprint(immed)
        exit(-1)
    code = '0x' + hex(code)[2:].zfill(digits)
    mask = '0x' + hex(mask)[2:].zfill(digits)
    bytes = pos // 8

    # Parse register list
    rv = []
    tv = []
    for r in reglist.split(','):
        if r == '-':
            rv.append('NOREG')
            tv.append('-')
            continue
        elif r[0].isnumeric():
            rv.append(r)
            tv.append('l')
            continue
        m = regspecpat.match(r)
        if not m:
            eprint("bad register specifier", r)
            exit(-1)
        ty, regn = m.groups()
        (hi, lo, plus) = regnames[regn]
        typ = '+GPREG'
        if ty == 'f' or ty == 'd':
            typ = '+FPREG'
#        else:
#            eprint("bad register type specifier", r)
#            exit(-1)
        t = 'x({:d},{:d}){:s}{:s}'.format(lo, hi-lo+1, plus, typ)
        if t not in regspec:
            regspec[t] = numregspecs
            numregspecs += 1
        t = 'regspec[{:d}](b)'.format(regspec[t])
        rv.append(t)
        tv.append(ty)
    while len(reglist) < 4:
        rv.append('NOREG')
        tv.append('-')

    instructions[opcode] = (opname, asm, attr, code, mask, bytes, immed, immtyp, rv, action, tv)

    insn_in_order.append(opcode)
    if bytes == 2:
        last_compressed_opcode = opcode
#    print(instructions[opcode])

    if bytes == 4:
        bytes = False
    else:
        bytes = True
    if immtyp == 0:
        immtyp = False
    else:
        immtyp = True
    instruction_struct[opcode] = InsnStruct(name = opname,
                                            asm = asm,
                                            attr = attr,
                                            code = code,
                                            mask = mask,
                                            compressed = bytes,
                                            immed = immed,
                                            longimm = immtyp,
                                            action = action,
                                            regspecs = rv,
                                            regtypes = tv)


opcodes = ['ZERO'] + [key for key in instructions] + ['ILLEGAL', 'UNKNOWN']

with open('newcode.tmp', 'w') as f:
    json.dump(instructions, f)
diffcp('isa.json')

ATTR = {}
ISA  = {}
for opcode, t in instructions.items():
    (opname, asm, attr, code, mask, bytes, immed, immtyp, reglist, action, regtypes) = t
    for a in attr:
        if a == '-':
            continue
        if a.isnumeric() or len(a)==1 and a.isupper():
            ISA[a] = 1
        elif a.isalpha():
            ATTR[a] = 1;

def make_mask(key):
    val = []
    mask = 0
    n = 0
    for opcode in opcodes:
        if n == 64:
            val.append(mask)
            mask = 0
            n = 0
        if opcode in instructions and key in instructions[opcode][2]:
            mask |= 1 << n
        n += 1
    val.append(mask)
    return val

def emit_bitvec(table, name):
    f.write('const {:s}_bv_t {:s}[] = {{\n'.format(name, name))
    for opcode in opcodes:
        bv = []
        if opcode in instructions:
            for t in instructions[opcode][2]: # list of attributes
                if t in table:
                    bv.append('{:s}_{:s}'.format(name, t))
        bv = '|'.join(bv)
        if len(bv) == 0:
            bv = '0'
        f.write('  /* {:19s} */ {:20s}\n'.format(opcode, bv+','))
    f.write('};\n\n')
    

with open('newcode.tmp', 'w') as f:
    f.write('const char* op_name[] = {')
    n = 0
    for opcode in opcodes:
        if n % 4 == 0:
            f.write('\n  ')
        f.write('{:20s}'.format('"' + opcode.replace('.','_') + '",'))
        n += 1
    f.write('\n};\n\n')

    emit_bitvec(ATTR, 'ATTR')
    emit_bitvec(ISA, 'ISA')
    f.write('const uint64_t stop_before[] = {\n')
    for t in make_mask('<'):
        f.write('  0x{:016x},\n'.format(t))
    f.write('};\n\n')
    f.write('const uint64_t stop_after[] = {\n')
    for t in make_mask('>'):
        f.write('  0x{:016x},\n'.format(t))
    f.write('};\n\n')
diffcp('../caveat/constants.h')

def makelist(table, n, f, name):
    list = [ None ] * n
    for t in table.keys():
        list[table[t]] = t

    f.write('int (*const {:s}[])(int32_t b) = {{'.format(name))
    for k in range(len(list)):
        f.write('\n  /* {:2d} */  [](int32_t b) {{ return {:s}; }},'.format(k, list[k]))
    f.write('\n};\n\n')
    
with open('newcode.tmp', 'w') as f:
    makelist(regspec, numregspecs, f, 'regspec')
    makelist(immspec, numimmspecs, f, 'immspec')
    for opcode in insn_in_order:
        (opname, asm, attr, code, mask, bytes, immed, immtyp, reglist, action, regtypes) = instructions[opcode]
        if immtyp == 1:
            f.write('  if (match(b, {:s}, {:s}, {:15s}, i, {:15s}, {:15s}, {:15s})) return i;\n' \
                    .format(mask, code, opname, reglist[0], reglist[1], immed))
        else:
            f.write('  if (match(b, {:s}, {:s}, {:15s}, i, {:15s}, {:15s}, {:15s}, {:15s}, {:15s})) return i;\n' \
                    .format(mask, code, opname, reglist[0], reglist[1], reglist[2], reglist[3], immed))
diffcp('../caveat/decoder.h')

with open('newcode.tmp', 'w') as f:
    for opcode, t in instructions.items():
        (opname, asm, attr, code, mask, bytes, immed, immtyp, reglist, action, regtypes) = t
        if 'spike' in attr:
            fini = 'break'
            if 'cj' in attr or 'uj' in attr:
                fini = 'spike_stop'
            f.write('    case {:20s} {:s}; {:s}; // len={:d}\n'.format(opname+':', action, fini, int(bytes)))
        else:
            f.write('    case {:20s} {:s}; s.pc+={:d}; break;\n'.format(opname+':', action, int(bytes)))
diffcp('../caveat/semantics.h')
    

exit(0)
print('register specifiers')
for t in sorted(regspecs.keys()):
    print(t)
print('\nimmediates')
for t in sorted(immspecs.keys()):
    print(t)
