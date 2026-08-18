/*
  Copyright (c) 2023 Peter Hsu.  All Rights Reserved.  See LICENCE file for details.
*/
#include <cmath>


#define RM ({ int rm = i->immed(); \
              if(rm == 7) rm = s.frm; \
              if(rm > 4) die("Illegal instruction"); \
              rm; })
#undef RM

#define srm
#define sfx

#define boxf(x) (0xFFFFFFFF00000000L | (x))

// RISC-V sign-injection and classify instructionsn

#define F32_SIGN ((unsigned       int)1 << 31)
#define F64_SIGN ((unsigned long long)1 << 63)

static inline unsigned int fsgnj_s(unsigned int a, unsigned int b, bool n, bool x)
{
  return (a & ~F32_SIGN) | ((((x) ? a : (n) ? F32_SIGN : 0) ^ b) & F32_SIGN);
}

static inline unsigned long long fsgnj_d(unsigned long long a, unsigned long long b, bool n, bool x)
{
  return (a & ~F64_SIGN) | ((((x) ? a : (n) ? F64_SIGN : 0) ^ b) & F64_SIGN);
}

static inline unsigned classify_s(float aa)
{
  reg_t a;
  a.f = aa;
  int sign = a.u & F32_SIGN;
  switch (std::fpclassify(aa)) {
  case FP_INFINITE:	return sign ? 0 : 7;
  case FP_NAN:		return            9;
  case FP_SUBNORMAL:	return sign ? 2 : 5;
  case FP_ZERO:		return sign ? 3 : 4;
  case FP_NORMAL:
  default:		return sign ? 1 : 6;
  }
}

static inline unsigned classify_d(double aa)
{
  reg_t a;
  a.d = aa;
  int sign = a.u & F64_SIGN;
  switch (std::fpclassify(aa)) {
  case FP_INFINITE:	return sign ? 0 : 7;
  case FP_NAN:		return            9;
  case FP_SUBNORMAL:	return sign ? 2 : 5;
  case FP_ZERO:		return sign ? 3 : 4;
  case FP_NORMAL:
  default:		return sign ? 1 : 6;
  }
}


// Integer multiplication routines

static inline uint64_t mulhu(uint64_t a, uint64_t b)
{
  uint64_t t;
  uint32_t y1, y2, y3;
  uint64_t a0 = (uint32_t)a, a1 = a >> 32;
  uint64_t b0 = (uint32_t)b, b1 = b >> 32;

  t = a1*b0 + ((a0*b0) >> 32);
  y1 = t;
  y2 = t >> 32;

  t = a0*b1 + y1;
  y1 = t;

  t = a1*b1 + y2 + (t >> 32);
  y2 = t;
  y3 = t >> 32;

  return ((uint64_t)y3 << 32) | y2;
}

static inline int64_t mulh(int64_t a, int64_t b)
{
  int negate = (a < 0) != (b < 0);
  uint64_t res = mulhu(a < 0 ? -a : a, b < 0 ? -b : b);
  return negate ? ~res + (a * b == 0) : res;
}

static inline int64_t mulhsu(int64_t a, uint64_t b)
{
  int negate = a < 0;
  uint64_t res = mulhu(a < 0 ? -a : a, b);
  return negate ? ~res + (a * b == 0) : res;
}

