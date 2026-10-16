import sys
import os
import re
import json
from dataclasses import dataclass

@dataclass
class Field:
    hi:		int
    lo:		int
fields = {}

@dataclass
class Format:
    bitpat:	str
    flags:	str
    asm:	str
    fmtdescr:	str
formats = {}

@dataclass
class Instruction:
    bitpat:	str
    flags:	str
    asm:	str
    action:	str
    descr:	str
    immed:	str = None
    code:	str = None
    mask:	str = None
insns = {}

@dataclass
class Hook:
    hi:		int
    lo:		int
    table:	any
hooks = {}
    
opcodes = []
mapping = {}

ignore_line = re.compile(r'^$|^#')
field_line  = re.compile(r'\s*(\w+)\[(\d+):(\d+)\]')
format_line = re.compile(r'^\s*(\w+)\s+\"(.*)\"\s+(\S+)\s+(\S+)\s+\"(.*)\"')
map_section = re.compile(r'^@map\s+(\w+)\[(\d+):(\d+)\]\s+\"\"\"')
op_explicit = re.compile(r'^\s*(\S+)\s+\"(.*)\"\s+(\S+)\s+(\S+)\s+\"(.*)\"(\s+\"(.*)\")?')
op_with_fmt = re.compile(r'^\s*(\S+)\s+(\S+)\s+\"(.*)\"\s+\"(.*)\"')

hook_spec = re.compile(r'^\?(\w+)\[(\d+)\]')
reg_type = re.compile(r'^([a-z_A-Z]\w*)(\.(.*))?')



with open(sys.argv[1]) as f:
    lines = f.readlines()
    while lines:
        line = lines.pop(0).strip()
        if re.match('^@fields', line):
            line = lines.pop(0).strip()
            while line != '"""':
                m = field_line.match(line)
                if m:
                    name, hi, lo = m.groups()
                    hi = int(hi)
                    lo = int(lo)
                    fields[name] = Field(hi, lo)
                elif not ignore_line.match(line):
                    print('unrecognized ', line)
                    exit(-1)
                line = lines.pop(0).strip()
        elif re.match('^@format', line):
            line = lines.pop(0).strip()
            while line != '"""':
                m = format_line.match(line)
                if m:
                    name, bitpat, flags, asm, fmtdescr = m.groups()
                    formats[name] = Format(bitpat, flags, asm, fmtdescr) 
                elif not ignore_line.match(line):
                    print('unrecognized ', line)
                line = lines.pop(0).strip()
            continue
        
        elif re.match('^@map', line):
            m = map_section.match(line)
            if not m:
                print('Bad map declaration: ', line)
                exit(-1)
            name, hi, lo = m.groups()
            hi = int(hi)
            lo = int(lo)
            if lo > hi:
                print('Bad bit positions', line)
                exit(-1)
            table = {}
            code = 0
            line = lines.pop(0).strip()
            while line != '"""':
                if not ignore_line.match(line):
                    tuple = line.partition('#')[0].split()
                    for t in tuple:
                        opcode = t
                        span = 0
                        if '/' in t:
                            opcode, span = t.split('/')
                        if opcode != '.':
                            table[opcode] = [code, span]
                            mapping[opcode] = name
                        code += 1 << int(span)
                line = lines.pop(0).strip()
            hooks[name] = Hook(hi, lo, table)
            continue    

        else:
            m = op_with_fmt.match(line)
            n = op_explicit.match(line)
            if m:
                name, fmt, action, descr = m.groups()
                tuple = fmt.split(',')
                format_name = tuple[0]
                if format_name not in formats:
                    print('Undefined format', format_name, 'in line')
                    print(line)
                    exit(-1)
                fmt = formats[format_name]
                tuple = fmt.flags.split(',') + tuple[1:]
                flags = ','.join(tuple)
                insns[name] = Instruction(fmt.bitpat, flags, fmt.asm, action, descr)
                opcodes.append(name)
            elif n:
                name, bitpat, flags, asm, action, dummy, descr = n.groups()
                insns[name] = Instruction(bitpat, flags, asm, action, descr)
                opcodes.append(name)
            elif not ignore_line.match(line):
                print('unrecognized ', line)
                exit(-1)


for opcode in insns:
    bitpat = insns[opcode].bitpat
    code, mask, pos = 0, 0, 0
    signed, longimm = False, False
    immed = []
    # process bit pattern chucks from right to left
    for b in reversed(bitpat.split()):
        if re.match('[01]+', b):
            code |= int(b, 2) << pos
            mask |= ((1<<len(b))-1) << pos
            pos += len(b)
        elif re.match('[.]+', b):
            pos += len(b)
        elif b[0] == '?':
            m = hook_spec.match(b)
            if not m:
                print('Bad hook', b, 'in', bitpat)
                exit(-1)
            name, width = m.groups()
            width = int(width)
            if name in hooks:
                h = hooks[name]
            elif name in mapping:
                h = hooks[mapping[name]]
            else:
                print('Unknown hook/opcode name', name, 'in', bitpat)
                exit(-1)
            if width != h.hi - h.lo + 1:
                print('Width mismatch', name, 'width', width, 'hi,lo', h.hi, h.lo, 'in', bitpat)
                exit(-1)
            value, span = h.table[opcode]
            code |= value << pos
            mask |= ((1 << (width-span)) -1) << pos
            pos += width
        elif re.match(r'^[a-z_A-Z]', b):
            m = reg_type.match(b)
            if not m:
                print('Bad register specifier', name, 'in', bitpat)
                exit(-1)
            name, dummy, typ = m.groups()
            if name not in fields:
                print('Unknown register field name', name, 'in', bitpat)
                exit(-1)
            pos += fields[name].hi - fields[name].lo + 1
        elif re.match(r'\{[^}]+\}', b):
            b = b[1:-1]
            tuple = []
            for t in reversed(b.split('|')):
                if t[0] == '-':
                    signed = True
                    t = t[1:]
                m = re.match(r'(\d+)(:(\d+))?', t)
                if not m:
                    print('Bad immediate specifier', t, 'in', bitpat)
                    exit(-1)
                hi, colon, lo = m.groups()
                hi = int(hi)
                lo = int(lo) if colon else hi
                if lo > hi:
                    print('Bad bit positions', t, 'in', bitpat)
                    exit(-1)
                if hi >= 13:
                    longimmed = True
                immed.append('x({:d},{:d})<<{:d}'.format(pos, hi-lo+1, lo))
                pos += hi-lo+1
        else:
            print('Unknown field', b, 'in bits "', bitpat, '"')
            exit(-1)
    if pos % 8 != 0:
        print('Length', pos, 'not multiple of 8 bits', bitpat)
        exit(-1)
    insns[opcode].length = int(pos / 8)
    insns[opcode].immed = immed
    insns[opcode].code = code
    insns[opcode].mask = mask

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

def eprint(*args):
    sys.stderr.write(' '.join(map(str,args)) + '\n')

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

def define_bitvec(table):
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
    
with open('newcode.tmp', 'w') as f:
    f.write('enum Opcode_t : short {')
    n = 0
    for opcode in opcodes:
        if n % 4 == 0:
            f.write('\n  ')
        f.write('{:20s}'.format('Op_' + opcode.replace('.','_') + ','))
        n += 1
    f.write('\n};\n\n')
    f.write('const Opcode_t Last_Compressed_Opcode = Op_{:s};\n'.format(last_compressed_opcode.replace('.','_')))
    f.write('const int Num1ber_of_Opcodes = {:d};\n\n'.format(len(opcodes)))

    bv = define_bitvec(ISA)
    f.write('enum ISA_t {')
    for t in sorted(ISA.keys()):
        f.write('\n  ISA_{:s} = 1 << {:d},'.format(t, ISA[t]))
    f.write('\n}};\ntypedef {:s} ISA_bv_t;\n\n'.format(bv))
    bv = define_bitvec(ATTR)
    f.write('enum ATTR_t {')
    for t in sorted(ATTR.keys()):
        f.write('\n  ATTR_{:s} = 1 << {:d},'.format(t, ATTR[t]))
    f.write('\n}};\ntypedef {:s} ATTR_bv_t;\n\n'.format(bv))
diffcp('../caveat/opcodes.h')

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
