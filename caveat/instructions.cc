/*
  Copyright (c) 2023 Peter Hsu.  All Rights Reserved.  See LICENCE file for details.
*/
#include <stdlib.h>
#include <stdint.h>
#include <stdio.h>
#include <sys/mman.h>
#include <sys/types.h>

#include <string>
#include <map>

#include "caveat.h"
#include "hart.h"

std::map<long, std::string> fname; // dictionary of pc->name

const char* func_name(Addr_t pc)
{
  return fname.count(pc)==1 ? const_cast<const char*>(fname.at(pc).c_str()) : "NOT FOUND";
}

#define LABEL_WIDTH  16
#define OFFSET_WIDTH  8
int slabelpc(char* buf, Addr_t pc)
{
  auto it = fname.upper_bound(pc);
  if (it == fname.end()) {
    return sprintf(buf, "%*.*s+%*ld %8lx: ", LABEL_WIDTH, LABEL_WIDTH, "NONE", -(OFFSET_WIDTH-1), 0L, pc);
  }
  else {
    it--;
    if (it == fname.end()) {
      return sprintf(buf, "%*.*s+%*ld %8lx: ", LABEL_WIDTH, LABEL_WIDTH, "UNKNOWN", -(OFFSET_WIDTH-1), 0L, pc);
    }
    else {
      long offset = pc - it->first;
      const char* name = const_cast<const char*>(it->second.c_str());
      return sprintf(buf, "%*.*s+%*ld %8lx: ", LABEL_WIDTH, LABEL_WIDTH, name, -(OFFSET_WIDTH-1), offset, pc);
    }
  }
}

void labelpc(Addr_t pc, FILE* f)
{
  char buffer[1024];
  slabelpc(buffer, pc);
  fprintf(f, "%s", buffer);
}

int sdisasm(char* buf, Addr_t pc, const Insn_t* i)
{
  int n = 0;
  if (i->opcode() == Op_ZERO) {
    n += sprintf(buf, "Nothing here");
    return n;
  }
  uint32_t b = *(uint32_t*)pc;
  switch (i->length()) {
  case 2:
    n += sprintf(buf+n, "    %04x  ", b&0xFFFF);
    break;
  case 4:
    n += sprintf(buf+n, "%08x  ",     b);
    break;
  default:
    die("Unknown instruction length %d", i->length());
  }
  n += sprintf(buf+n, "%-23s", op_name[i->opcode()]);
  char sep = ' ';
  if (i->rd()  != NOREG) { n += sprintf(buf+n, "%c%s", sep, reg_name[i->rd() ]); sep=','; }
  if (i->rs1() != NOREG) { n += sprintf(buf+n, "%c%s", sep, reg_name[i->rs1()]); sep=','; }
  if (i->longimmed())    { n += sprintf(buf+n, "%c%d(0x%x)", sep, i->immed(), i->immed()); }
  else {
    if (i->rs2() != NOREG) { n += sprintf(buf+n, "%c%s", sep, reg_name[i->rs2()]); sep=','; }
    if (i->rs3() != NOREG) { n += sprintf(buf+n, "%c%s", sep, reg_name[i->rs3()]); sep=','; }
    n += sprintf(buf+n, "%c%d(0x%x)", sep, i->immed(), i->immed());
  }
  return n;
}

void disasm(Addr_t pc, const Insn_t* i, const char* end, FILE* f)
{
  char buffer[1024];
  sdisasm(buffer, pc, i);
  fprintf(f, "%s%s", buffer, end);
}



int sprtinsn(char* buf, Addr_t pc, const Insn_t* i, Result_t result)
{
  int n = 0;
  if (i->rd() == NOREG) {
    if (ATTR[i->opcode()] & ATTR_st)
      n += sprintf(buf+n, "%4s[%016lx] ", reg_name[i->rs2()], result.data.ul); 
    else if ((ATTR[i->opcode()] & (ATTR_cj|ATTR_uj)) && (i->rs1() != NOREG))
      n += sprintf(buf+n, "%4s[%016lx] ", reg_name[i->rs1()], result.data.ul); 
    else if (ATTR[i->opcode()] & ATTR_ex)
      n += sprintf(buf+n, "%4s[%016lx] ", reg_name[10], result.data.ul);
    else
      n += sprintf(buf+n, "%4s[%16s] ", "", "");
  }
  else
    fprintf(stderr, "%4s[%016lx] ", reg_name[i->rd()], result.data.ul);
  n += slabelpc(buf+n, pc);
  n += sdisasm(buf+n, pc, i);
  if (ATTR[i->opcode()] & (ATTR_ld|ATTR_st|ATTR_rmw))
    n += sprintf(buf+n, ",vaddr=0x%lx ", result.addr);
  return n;
}

void prtinsn(Addr_t pc, const Insn_t* i, Result_t result, const char* end, FILE* f)
{
  char buffer[1024];
  sprtinsn(buffer, pc, i, result);
  fprintf(f, "%s%s", buffer, end);
}



#include "constants.h"
