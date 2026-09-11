exit(0)

import sys
import os
import re
import json

def diffcp(fname):
    if os.path.exists(fname) and os.system('cmp -s newcode.tmp '+fname) == 0:
        os.system('rm newcode.tmp')
    else:
        os.system('mv newcode.tmp '+fname)
    
repo = sys.argv[1]
if repo[-1] != '/':
    repo += '/'

with open('isa.json', 'r') as f:
    instructions = json.load(f)

#
# Make additions to riscv-opc.h files.
#
for fn in [repo+'gdb/include/opcode/riscv-opc.h', repo+'binutils/include/opcode/riscv-opc.h']:
    found = False
    with open(fn, 'r') as s, open('newcode.tmp', 'w') as f:
        line = s.readline()
        while line:
            # First remove any old stuff
            if line == '/* CAVA begin */\n':
                print(fn, ":  Removing old CAVA stuff");
                line = s.readline()
                while line != '/* CAVA end */\n':
                    line = s.readline()
                line = s.readline()
            # Look for known place to make addition
            if line == '/* Unprivileged Counter/Timers CSR addresses.  */\n':
                found = True
                f.write('/* CAVA begin */\n')
                for opcode in instructions:
                    (opname, asm, attr, code, mask, bytes, immed, immtyp, reglist, action, regtypes) = instructions[opcode]
                    if 'custom' not in attr:
                        continue
                    upper_op = opname.upper()
                    f.write('#define MATCH_{:s}  {:s}\n'.format(upper_op, code))
                    f.write('#define MASK_{:s}  {:s}\n'.format(upper_op, mask))
                f.write('/* CAVA end */\n')
            f.write(line)
            line = s.readline()
    if found:
        diffcp(fn)
    else:
        print(fn, ":  Did not find known place");
    
#
# Make additions to riscv-opc.c files.
#
for fn in [repo+'gdb/opcodes/riscv-opc.c', repo+'binutils/opcodes/riscv-opc.c']:
    found = False
    with open(fn, 'r') as s, open('newcode.tmp', 'w') as f:
        line = s.readline()
        while line:
            # First remove any old stuff
            if line == '/* CAVA begin */\n':
                print(fn, ":  Removing old CAVA stuff");
                line = s.readline()
                while line != '/* CAVA end */\n':
                    line = s.readline()
                line = s.readline()
            # Look for known place to make addition
            if line == '/* Atomic memory operation instruction subset.  */\n':
                found = True
                f.write('/* CAVA begin */\n')
                for opcode in instructions:
                    (opname, asm, attr, code, mask, bytes, immed, immtyp, reglist, action, regtypes) = instructions[opcode]
                    if 'custom' not in attr:
                        continue
                    upper_op = opname.upper()
                    clas = 'I'
                    f.write('{{{:17} {:2d}, INSN_CLASS_{:s}, {:11s} MATCH_{:s}, MASK_{:s}, match_opcode, 0 }},\n'
                            .format('"'+opcode+'",', 0, clas, '"'+asm+'",', upper_op, upper_op)) 
                f.write('/* CAVA end */\n')
            f.write(line)
            line = s.readline()
    if found:
        diffcp(fn)
    else:
        print(fn, ":  Did not find known place");

#
# Generate assembly header file
#
def getCtype(t):
    Ctype = []
    if "u" in t:
        Ctype.append('unsigned')
    if "b" in t:
        Ctype.append('char')
    elif "h" in t:
        Ctype.append('short')
    elif "w" in t:
        Ctype.append('int')
    elif "l" in t:
        Ctype.append('long')
    elif "f" in t:
        Ctype.append('float')
    elif "d" in t:
        Ctype.append('double')
    return ' '.join(Ctype)
    

with open('newcode.tmp', 'w') as f:
    for opcode, t in instructions.items():
        (opname, asm, attr, code, mask, bytes, immed, immtyp, eglist, action, regtypes) = t
        if 'custom' not in attr:
            continue
        inargs = []
        inputs = []
        asmargs = []
        n = 0
        t = getCtype(regtypes[0])
        if t == '':
            outtyp = 'void'
            output = '/* no output register */'
        else:
            outtyp = t
            regspec = 'r'
            if t=='float' or t=='double':
                regspec = 'f'
            output = '"={:s}"(y)'.format(regspec)
            asmargs.append('%{:d}'.format(n))
            n += 1
        for k in range(1, 4):
            t = getCtype(regtypes[k])
            if t:
                inargs.append('{:s} x{:d}'.format('const '+t, n))
                regspec = 'r'
                if t=='float' or t=='double':
                    regspec = 'f'
                inputs.append('"{:s}"(x{:d})'.format(regspec, n))
                asmargs.append('%{:d}'.format(n))
                n += 1
                
        f.write('inline {:s} {:s}({:s}) {{ \n'.format(outtyp, opcode, ', '.join(inargs)))
        if (outtyp != 'void'):
            f.write('  {:s} \n'.format(outtyp))
        f.write('  __asm__("{:s}\t{:s}" \n'.format(opcode, ','.join(asmargs)))
        f.write('\t: {:s} \n'.format(output))
        f.write('\t: {:s} \n'.format(','.join(inputs)))
        if 'st' in attr or 'amo' in attr:
            f.write('\t: "memory");\n')
        else:
            f.write('\t: /* no clobber */);\n')
        if (outtyp != 'void'):
            f.write('  return y; \n')
        f.write('};\n\n')
diffcp('custom_asm_macros.h')
