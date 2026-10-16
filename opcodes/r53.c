/* r53.c - Machine specific routines for the R53 architecture.
   Copyright 2026 by Peter Hsu
*/

#include "config.h"
#include "system.h"
#include "coretypes.h"
#include "tm.h"
#include "rtl.h"
#include "regs.h"
#include "hard-reg-set.h"
#include "real.h"
#include "insn-config.h"
#include "conditions.h"
#include "insn-attr.h"
#include "flags.h"
#include "recog.h"
#include "obstack.h"
#include "tree.h"
#include "expr.h"
#include "optabs.h"
#include "except.h"
#include "function.h"
#include "output.h"
#include "basic-block.h"
#include "integrate.h"
#include "toplev.h"
#include "ggc.h"
#include "hashtab.h"
#include "tm_p.h"
#include "target.h"
#include "target-def.h"
#include "langhooks.h"
#include "reload.h"
#include "cfglayout.h"
#include "sched-int.h"
#include "gimple.h"
#include "tree-flow.h"
#include "intl.h"
#include "params.h"
#include "tm-constrs.h"



/* Abort after printing out a specific insn.  */

static void
abort_with_insn (rtx insn, const char reason[])
{
  error(reason);
  debug_rtx(insn);
  abort();
}






/*-----------------------------------------------------------------------------

	Register Usage.  See callUsedRegs[] for which registers are
	preserved and which are clobbered across function calls.

  a0		sp stack pointer
  a1		gp global storage pointer
  a2		tp thread-local storage pointer
  a3		fp frame pointer, if necessary (e.g. alloca)
  a4		ap argument pointer, if necessary
  a5		static chain, if applicable
  a6,a7,s0..s6	scratch registers, values not saved
  s7		ra return address
  t0..t47	scratech registers, values saved across call


			Stack Layout

             Just before call               Just after prologue           ^
         |                       |       |                       |        |
         +-----------------------+       +-----------------------+       High 
         |  local variables,     |       |                       |      Memory
         |  reg save area, etc.  |       |                       |
         +-----------------------+ <-FP  +-----------------------+
         |   space created by    |       |                       |
         |   `alloca', if any    |       |                       |
         +-----------------------+       +-----------------------+
         | args to the func that |       |                       |
         | is being called that  |       |  args to this func.   |
         | do not fit in regs    |       |                       |
         +-----------------------+ <-SP  +-----------------------+
                                       / |  args that used to be |
 current_function_pretend_args_size   |  | in regs; only created |
                                       \ |   for vararg funcs    |
                                         +-----------------------+ <-AP (always
                                       / | return address    $31 |  eliminated)
                                      |  | prev. frame ptr   $30 |
                     r53_save_area() |  | call preserved     .  |
                                       \ |   registers        .  |
                                         +-----------------------+
                                       / |                       |
    ROUND_AUTO_AREA(get_frame_size()) |  |   local variables     |
                                       \ |                       |
                                         +-----------------------+ <-FP (may be
                                       / |  room for args to     |  eliminated)
current_function_outgoing_args_size   |  |  other funcs called   |
                                       \ |  from this one        |
                                         +-----------------------+ <-SP
*/

#define R53_NUM_ARG_REGS   6
/*   Number of registers used for argument passing. */



/*   Stack is rounded to 16-byte boundary.  */
#define ROUND_AUTO_AREA(x)   ((((x) + 0xf) / 0xf) * 0xf)

static int r53_saved_reg[R53_GENERAL_REGS];

static int
r53_save_area(void) {
    int save_area = 0;
    int r;

    /* Figure out which registers need saving. */
    for (r=0; r<R53_GENERAL_REGS; r++) {
	r53_saved_reg[r] = 0;
	/* Call clobbered registers need never be saved. */
	if (call_used_regs[r])
	    continue;
	/* Some uses of $fp and $ra seem to be invisible. */
	if (!df_regs_ever_live_p(r)
	    && !(r == FRAME_POINTER_REGNUM && frame_pointer_needed)
	    && !(r == RETURN_ADDR_REGNUM && profile_flag))
	    continue;
	/* Need to save during prologue. */
	r53_saved_reg[r] = 1;
	save_area += UNITS_PER_WORD;
    }

    return save_area;
}

static void
emit_constant(rtx into, long long value) {
  if (CONST_OK_FOR_LETTER_P(value, 'L'))
    emit_insn(gen_rtx_SET(Pmode, into, GEN_INT(value)));
  else {
    long long low =  value << 53 >> 53; /* 11 bit immediate */
    long long high = value - low;
    emit_insn(gen_rtx_SET(Pmode, into, GEN_INT(high)));
    emit_insn((Pmode==SImode?gen_addsi3:gen_adddi3)(into, into, GEN_INT(low)));
  }
}


void
r53_expand_prologue(void) {
    int auto_area = ROUND_AUTO_AREA(get_frame_size());
    int save_pretend = crtl->args.pretend_args_size + r53_save_area();
    int frame_size = save_pretend + auto_area + crtl->outgoing_args_size;

    if (frame_size > 0) {
	int offset;
	int r;

	if (CONST_OK_FOR_LETTER_P (frame_size, 'M')) {
	    emit_insn((Pmode==DImode ? gen_adddi3 : gen_addsi3) (stack_pointer_rtx, stack_pointer_rtx, GEN_INT(-frame_size)));
	    offset = frame_size - save_pretend;
	}
	else {			/* Allocate big frames in two steps */
  	    fprintf(stderr, "***** big frame: %d, save_pretend=%d\n", frame_size, save_pretend);
	    emit_insn((Pmode==DImode ? gen_adddi3 : gen_addsi3) (stack_pointer_rtx, stack_pointer_rtx, GEN_INT(-save_pretend)));
	    offset = 0;
	}
	/*
	 * Save call-preserved registers including fp and ra, as necessary.
	 * `offset' points to beginning of register save area.
	 */
	r = R53_GENERAL_REGS;
	while (--r >= 0) {
	    if (r53_saved_reg[r]) {
		emit_move_insn (gen_rtx_MEM (DImode, gen_rtx_PLUS (Pmode, stack_pointer_rtx, GEN_INT (offset))), gen_rtx_REG(DImode, r));
		offset += UNITS_PER_WORD;
	    }
	}
	/*
	 * Put `varargs' arguments into memory, if necessary.
	 * Note `offset' already points to correct place.
	 */
	if (crtl->args.pretend_args_size > 0) {
	    int nregs = crtl->args.pretend_args_size / UNITS_PER_WORD;
	    for (r=R53_NUM_ARG_REGS-nregs; r<R53_NUM_ARG_REGS; r++) {
		emit_move_insn (gen_rtx_MEM (DImode, gen_rtx_PLUS (Pmode, stack_pointer_rtx, GEN_INT (offset))), gen_rtx_REG(DImode, r+1));
		offset += UNITS_PER_WORD;
	    }
	}
    }
    /*
     * Second part of big frame.
     */
    if (!CONST_OK_FOR_LETTER_P (frame_size, 'M')) {
	rtx tmp = gen_rtx_REG (Pmode, 7); /* Scratch register */
	emit_constant(tmp, -(frame_size - save_pretend));
	emit_insn ((Pmode==DImode ? gen_adddi3 : gen_addsi3) (stack_pointer_rtx, stack_pointer_rtx, tmp));
    }
    /*
     * Finally, create new frame pointer, if needed.
     */
    if (frame_pointer_needed || cfun->calls_alloca) {
	if (crtl->outgoing_args_size == 0)
	    emit_move_insn(frame_pointer_rtx, stack_pointer_rtx);
	else {
	    emit_move_insn(frame_pointer_rtx, GEN_INT( crtl->outgoing_args_size));
	    emit_insn(((Pmode==DImode) ? gen_adddi3 : gen_addsi3)
			  (frame_pointer_rtx, frame_pointer_rtx, stack_pointer_rtx));
	}
    }
}

void
r53_expand_epilogue(void) {
    int auto_area = ROUND_AUTO_AREA(get_frame_size());
    int save_pretend = crtl->args.pretend_args_size + r53_save_area();
    int frame_size = save_pretend + auto_area + crtl->outgoing_args_size;
    int adjustment = frame_size;
    int offset = frame_size - save_pretend;
    /*
     * If stack pointer has been altered by alloca then reload it
     * from frame pointer.  Remember to account for lack of
     * `outgoing_args_size' as a result.
     */
    if (cfun->calls_alloca) {
	emit_move_insn(stack_pointer_rtx, frame_pointer_rtx);
	adjustment -= crtl->outgoing_args_size;
	offset -= crtl->outgoing_args_size;
    }
    if (adjustment > 0) {
	int r;
	if (! CONST_OK_FOR_LETTER_P (adjustment, 'M')) { /* Big frame, do in two parts */
	    rtx tmp = gen_rtx_REG (Pmode, 7); /* Scratch register */
	    emit_constant(tmp, adjustment - save_pretend);
	    emit_insn ((Pmode==DImode ? gen_adddi3 : gen_addsi3) (stack_pointer_rtx, stack_pointer_rtx, tmp));
	    adjustment = save_pretend;
	    offset = 0;
	}
	/*
	 * Restore call-preserved registers.  Note `offset' points to beginning of register save area.
	 */
	r = R53_GENERAL_REGS;
	while (--r >= 0) {
	    if (r53_saved_reg[r]) {
		emit_move_insn (gen_rtx_REG(DImode, r), gen_rtx_MEM (DImode, gen_rtx_PLUS (Pmode, stack_pointer_rtx, GEN_INT (offset))));
		offset += UNITS_PER_WORD;
	    }
	}
	emit_insn((Pmode==DImode ? gen_adddi3 : gen_addsi3) (stack_pointer_rtx, stack_pointer_rtx, GEN_INT(adjustment)));
    }
}

int
r53_vector_mode_supported_p(enum machine_mode mode)
{
  switch (mode) {
  default:
    return 0;
  }
}

/* Return number of consecutive hard regs needed starting at reg REGNO
   to hold something of mode MODE.
   This is ordinarily the length in words of a value of mode MODE
   but can be less for certain modes in special long registers.
*/
int
r53_hard_regno_nregs(int regno, enum machine_mode mode)
{
  return 1;
}

int
r53_hard_regno_mode_ok(int regno, enum machine_mode mode)
{
  return (regno + HARD_REGNO_NREGS(regno, mode) - 1) < R53_GENERAL_REGS;
}

int
r53_modes_tieable_p(enum machine_mode mode1, enum machine_mode mode2)
{
  return 1;
}

int
r53_constant_alignment(tree exp, int align)
{
  return align;
}


int
r53_data_alignment (tree type, int align)
{
  return align;
}
/* Compute the alignment for a local variable or a stack slot.  EXP is
   the data type or decl itself, MODE is the widest mode available and
   ALIGN is the alignment that the object would ordinarily have.  The
   value of this macro is used instead of that alignment to align the
   object.  */

unsigned int
r53_local_alignment (tree exp, enum machine_mode mode ATTRIBUTE_UNUSED,
		      unsigned int align)
{
  tree type, decl;

  if (exp && DECL_P (exp))
    {
      type = TREE_TYPE (exp);
      decl = exp;
    }
  else
    {
      type = exp;
      decl = NULL;
    }
  /* If TYPE is NULL, we are allocating a stack slot for caller-save
     register in MODE.  We will return the largest alignment of XF
     and DF.  */
  if (!type)
    return align;
  
  return align;
}

/*   A macro to update M and UNSIGNEDP when an object whose type is
     TYPE and which has the specified mode and signedness is to be
     stored in a register.  This macro is only called when TYPE is a
     scalar type.

     On most RISC machines, which only have operations that operate on
     a full register, define this macro to set M to `word_mode' if M is
     an integer mode narrower than `BITS_PER_WORD'.  In most cases,
     only integer modes should be widened because wider-precision
     floating-point operations are usually more expensive than their
     narrower counterparts.

     For most machines, the macro definition does not change UNSIGNEDP.
     However, some machines, have instructions that preferentially
     handle either signed or unsigned quantities of certain modes.  For
     example, on the DEC Alpha, 32-bit loads from memory and 32-bit add
     instructions sign-extend the result to 64 bits.  On such machines,
     set UNSIGNEDP according to which kind of extension is more
     efficient.

     Do not define this macro if it would never modify M.   */
void
r53_promote_mode(enum machine_mode *mode,
		  int *unsignedp ATTRIBUTE_UNUSED)
{
    if (GET_MODE_CLASS(*mode) == MODE_INT
	&& GET_MODE_BITSIZE(*mode) < GET_MODE_BITSIZE(SImode))
    {
	*mode = SImode;
    }
}


/*   A C expression whose value is a register class containing hard
     register REGNO.  In general there is more than one such class;
     choose a class which is "minimal", meaning that no smaller class
     also contains the register.   */
enum reg_class
r53_regno_reg_class(int regno)
{
  if (regno < 8)
    return A_REGS;
  else if (regno < 16)
    return S_REGS;
  else if (regno < 64)
    return U_REGS;
  else
    return NO_REGS;
}


/*   A C expression which defines the machine-dependent operand
     constraint letters for register classes.  If CHAR is such a
     letter, the value should be the register class corresponding to
     it.  Otherwise, the value should be `NO_REGS'.  The register
     letter `r', corresponding to class `GENERAL_REGS', will not be
     passed to this macro; you do not need to handle it.   */
enum reg_class
r53_reg_class_from_letter(int letter)
{
  switch (letter) {
  case 'a':  return A_REGS;
  case 's':  return S_REGS;
  case 'r':  return GENERAL_REGS;
  case 't':  return T_REGS;
  default:   return NO_REGS;
  }
}



/*   A C expression that is nonzero if X is a legitimate constant for
     an immediate operand on the target machine.  You can assume that X
     satisfies `CONSTANT_P', so you need not check this.  In fact, `1'
     is a suitable definition for this macro on machines where anything
     `CONSTANT_P' is valid.   */
int
r53_legitimate_constant_p(enum machine_mode mode ATTRIBUTE_UNUSED, rtx x)
{
  /* maximum 28-bit signed integer */
  return GET_CODE(x)==CONST_INT && ((INTVAL(x)>>27)==0LL || (INTVAL(x)>>27)==-1LL);
}



/*   A C expression that defines the machine-dependent operand
     constraint letters (`I', `J', `K', ... `P') that specify
     particular ranges of integer values.  If C is one of those
     letters, the expression should check that VALUE, an integer, is in
     the appropriate range and return 1 if so, 0 otherwise.  If C is
     not one of those letters, the value should be 0 regardless of
     VALUE.   */
int
r53_const_ok_for_letter_p(
long long value,
int letter)
{
    switch (letter) {
    case 'I':  return -(1LL<<11) <= value && value < (1LL<<11);
    case 'J':  return -(1LL<<6)  <= value && value < (1LL<<6);
    case 'P':  return ((value>>27)==0LL || (value>>27)==-1LL);
    default:   return 0;
    }
}


/*   A C expression that defines the machine-dependent operand
     constraint letters that specify particular ranges of
     `const_double' values (`G' or `H').

     If C is one of those letters, the expression should check that
     VALUE, an RTX of code `const_double', is in the appropriate range
     and return 1 if so, 0 otherwise.  If C is not one of those
     letters, the value should be 0 regardless of VALUE.

     `const_double' is used for all floating-point constants and for
     `DImode' fixed-point constants.  A given letter can accept either
     or both kinds of values.  It can use `GET_MODE' to distinguish
     between these kinds.   */
int
r53_const_double_ok_for_letter_p(
rtx value,
int letter)
{
    switch (letter) {
    case 'G':  return value == CONST0_RTX(GET_MODE(value));
    default:   return 0;
    }
}


int
r53_extra_constraint(rtx x, int letter, int strict)
{
  rtx addr, base, disp;
  /*HOST_WIDE_INT offset;*/
  int offset;
  if (GET_CODE(x) != MEM)	/* used like ... "memory_operand" "R" */
    return 0;
  enum machine_mode mode = GET_MODE(x);
  addr = XEXP(x, 0);
  /* Bare register is ok--means displacement of zero. */
  if (register_operand(addr, Pmode))
    return REG_OK_FOR_BASE_P(addr);
  /* Otherwise may be reg+offset or reg+reg. */
  if (GET_CODE(addr) != PLUS && GET_CODE(addr) != LO_SUM)
    return 0;
  base = XEXP(addr, 0);
  if (!register_operand(base, Pmode))
    return 0;
  if ((strict && !REG_OK_FOR_BASE_STRICT_P(base)) || (!strict && !REG_OK_FOR_BASE_NONSTRICT_P(base)))
    return 0;
  disp = XEXP(addr, 1);
  if (GET_CODE(disp) == CONST_INT && GET_CODE(addr) == PLUS) {
    offset = INTVAL(disp);
    switch (letter) {
    case 'S': return CONST_OK_FOR_LETTER_P(offset, 'M');
    case 'R': return (0 <= offset && offset < (1<<12)) && (offset & 0x3F) == 0;
    default:  return 0;
    }
  }
  if (GET_CODE(disp) == LABEL_REF || GET_CODE(disp) == SYMBOL_REF || GET_CODE(disp) == CONST) {
    return GET_CODE(addr) == LO_SUM;
  }
  if (GET_CODE(disp) == REG || GET_CODE(disp) == SUBREG) {
    if (letter != 'T')
      return 0;
    if (GET_CODE(disp) == SUBREG)
      disp = XEXP(disp, 0);
    if (GET_CODE(disp) != REG)
      return 0;
    return ((strict && REG_OK_FOR_INDEX_STRICT_P(disp)) || (!strict && REG_OK_FOR_INDEX_NONSTRICT_P(disp)));
  }
  return 0;
}





/*   A C expression whose value is RTL representing the value of the
     return address for the frame COUNT steps up from the current
     frame, after the prologue.  FRAMEADDR is the frame pointer of the
     COUNT frame, or the frame pointer of the COUNT - 1 frame if
     `RETURN_ADDR_IN_PREVIOUS_FRAME' is defined.

     The value of the expression must always be the correct address when
     COUNT is zero, but may be `NULL_RTX' if there is not way to
     determine the return address of other frames.   */
rtx
r53_return_addr_rtx(
int count,
rtx frame_addr ATTRIBUTE_UNUSED)
{
    if (count != 0)
      return const0_rtx;
    else
      return get_hard_reg_initial_val (Pmode, 15);
}





/*   This macro is similar to `INITIAL_FRAME_POINTER_OFFSET'.  It
     specifies the initial difference between the specified pair of
     registers.  This macro must be defined if `ELIMINABLE_REGS' is
     defined.   */
int
r53_initial_elimination_offset(int from, int to)
{
    if (from == ARG_POINTER_REGNUM && to == STACK_POINTER_REGNUM)
	return (r53_save_area() +
		ROUND_AUTO_AREA(get_frame_size()) +
		crtl->outgoing_args_size);

    else if (from == ARG_POINTER_REGNUM && to == FRAME_POINTER_REGNUM)
	return (r53_save_area() +
		ROUND_AUTO_AREA(get_frame_size()));

    else if (from == FRAME_POINTER_REGNUM && to == STACK_POINTER_REGNUM)
	return (crtl->outgoing_args_size);

    else
	abort();
}

/*   A C expression that controls whether a function argument is passed
     in a register, and which register.

     The arguments are CUM, which summarizes all the previous
     arguments; MODE, the machine mode of the argument; TYPE, the data
     type of the argument as a tree node or 0 if that is not known
     (which happens for C support library functions); and NAMED, which
     is 1 for an ordinary argument and 0 for nameless arguments that
     correspond to `...' in the called function's prototype.  TYPE can
     be an incomplete type if a syntax error has previously occurred.

     The value of the expression is usually either a `reg' RTX for the
     hard register in which to pass the argument, or zero to pass the
     argument on the stack.

     For machines like the Vax and 68000, where normally all arguments
     are pushed, zero suffices as a definition.

     The value of the expression can also be a `parallel' RTX.  This is
     used when an argument is passed in multiple locations.  The mode
     of the of the `parallel' should be the mode of the entire
     argument.  The `parallel' holds any number of `expr_list' pairs;
     each one describes where part of the argument is passed.  In each
     `expr_list' the first operand must be a `reg' RTX for the hard
     register in which to pass this part of the argument, and the mode
     of the register RTX indicates how large this part of the argument
     is.  The second operand of the `expr_list' is a `const_int' which
     gives the offset in bytes into the entire argument of where this
     part starts.  As a special exception the first `expr_list' in the
     `parallel' RTX may have a first operand of zero.  This indicates
     that the entire argument is also stored on the stack.

     The usual way to make the ISO library `stdarg.h' work on a machine
     where some arguments are usually passed in registers, is to cause
     nameless arguments to be passed on the stack instead.  This is done
     by making `FUNCTION_ARG' return 0 whenever NAMED is 0.

     You may use the macro `MUST_PASS_IN_STACK (MODE, TYPE)' in the
     definition of this macro to determine if this argument is of a
     type that must be passed in the stack.  If `REG_PARM_STACK_SPACE'
     is not defined and `FUNCTION_ARG' returns non-zero for such an
     argument, the compiler will abort.  If `REG_PARM_STACK_SPACE' is
     defined, the argument will be computed in the stack and then
     loaded into a register.   */
rtx
r53_function_arg(
CUMULATIVE_ARGS* cum,
enum machine_mode mode,
tree type ATTRIBUTE_UNUSED,
int named ATTRIBUTE_UNUSED)
{
    if (*cum + HARD_REGNO_NREGS(0, mode) <= R53_NUM_ARG_REGS)
	return gen_rtx_REG(mode, (*cum)+1);
    else
	return NULL_RTX;
}



/*   A C statement (sans semicolon) to update the summarizer variable
     CUM to advance past an argument in the argument list.  The values
     MODE, TYPE and NAMED describe that argument.  Once this is done,
     the variable CUM is suitable for analyzing the _following_
     argument with `FUNCTION_ARG', etc.

     This macro need not do anything if the argument in question was
     passed on the stack.  The compiler knows how to track the amount
     of stack space used for arguments without any special help.   */
void
r53_function_arg_advance(
CUMULATIVE_ARGS* cum,
enum machine_mode mode_arg,
tree type_arg,
int named ATTRIBUTE_UNUSED)
{
  /*if (MUST_PASS_IN_STACK(mode, type))*/
  if (targetm.calls.must_pass_in_stack(mode_arg, type_arg))
	*cum = R53_NUM_ARG_REGS;
    else
	*cum += HARD_REGNO_NREGS(0, mode_arg);
}

/*   A C expression that is nonzero if REGNO is the number of a hard
     register in which function arguments are sometimes passed.  This
     does _not_ include implicit arguments such as the static chain and
     the structure-value address.  On many machines, no registers can be
     used for this purpose since all function arguments are pushed on
     the stack.   */
int
r53_function_arg_regno_p(
int regno)
{
    return (1 <= (regno) && (regno) <= R53_NUM_ARG_REGS);
}



/*   A C expression to create an RTX representing the place where a
     function returns a value of data type VALTYPE.  VALTYPE is a tree
     node representing a data type.  Write `TYPE_MODE (VALTYPE)' to get
     the machine mode used to represent that type.  On many machines,
     only the mode is relevant.  (Actually, on most machines, scalar
     values are returned in the same place regardless of mode).

     The value of the expression is usually a `reg' RTX for the hard
     register where the return value is stored.  The value can also be a
     `parallel' RTX, if the return value is in multiple places.  See
     `FUNCTION_ARG' for an explanation of the `parallel' form.

     If `PROMOTE_FUNCTION_RETURN' is defined, you must apply the same
     promotion rules specified in `PROMOTE_MODE' if VALTYPE is a scalar
     type.

     If the precise function being called is known, FUNC is a tree node
     (`FUNCTION_DECL') for it; otherwise, FUNC is a null pointer.  This
     makes it possible to use a different value-returning convention
     for specific functions when all their calls are known.

     `FUNCTION_VALUE' is not used for return vales with aggregate data
     types, because these are returned in another way.  See
     `STRUCT_VALUE_REGNUM' and related macros, below.   */
rtx
r53_function_value(
     tree ret_type,
     tree fn_decl_or_type ATTRIBUTE_UNUSED,
     bool outgoing ATTRIBUTE_UNUSED)
{
  enum machine_mode mode = TYPE_MODE(ret_type);
  return  gen_rtx_REG(mode, 1);

  if (GET_MODE_CLASS(mode) == MODE_INT &&
      GET_MODE_BITSIZE(mode) < GET_MODE_BITSIZE(SImode))
    mode = SImode;
  return gen_rtx_REG(mode, 1);
}


/*   A C expression to create an RTX representing the place where a
     library function returns a value of mode MODE.  If the precise
     function being called is known, FUNC is a tree node
     (`FUNCTION_DECL') for it; otherwise, FUNC is a null pointer.  This
     makes it possible to use a different value-returning convention
     for specific functions when all their calls are known.

     Note that "library function" in this context means a compiler
     support routine, used to perform arithmetic, whose name is known
     specially by the compiler and was not mentioned in the C code being
     compiled.

     The definition of `LIBRARY_VALUE' need not be concerned aggregate
     data types, because none of the library functions returns such
     types.   */
rtx
r53_libcall_value(
enum machine_mode mode)
{
    return gen_rtx_REG(mode, 1);
}

/*   This macro offers an alternative to using `__builtin_saveregs' and
     defining the macro `EXPAND_BUILTIN_SAVEREGS'.  Use it to store the
     anonymous register arguments into the stack so that all the
     arguments appear to have been passed consecutively on the stack.
     Once this is done, you can use the standard implementation of
     varargs that works for machines that pass all their arguments on
     the stack.

     The argument ARGS_SO_FAR is the `CUMULATIVE_ARGS' data structure,
     containing the values that are obtained after processing the named
     arguments.  The arguments MODE and TYPE describe the last named
     argument--its machine mode and its data type as a tree node.

     The macro implementation should do two things: first, push onto the
     stack all the argument registers _not_ used for the named
     arguments, and second, store the size of the data thus pushed into
     the `int'-valued variable whose name is supplied as the argument
     PRETEND_ARGS_SIZE.  The value that you store here will serve as
     additional offset for setting up the stack frame.

     Because you must generate code to push the anonymous arguments at
     compile time without knowing their data types,
     `SETUP_INCOMING_VARARGS' is only useful on machines that have just
     a single category of argument register and use it uniformly for
     all data types.

     If the argument SECOND_TIME is nonzero, it means that the
     arguments of the function are being analyzed for the second time.
     This happens for an inline function, which is not actually
     compiled until the end of the source file.  The macro
     `SETUP_INCOMING_VARARGS' should not generate any instructions in
     this case.   */

/*   All the work is actually done by `expand_prologue'.   */
void
r53_setup_incoming_varargs(
     CUMULATIVE_ARGS* sofar,
     enum machine_mode mode_arg ATTRIBUTE_UNUSED,
     tree type_arg ATTRIBUTE_UNUSED,
     int* pretend_size,
     int second_time ATTRIBUTE_UNUSED)
{
    if (*sofar < R53_NUM_ARG_REGS)
	*pretend_size = (R53_NUM_ARG_REGS - (*sofar+1)) * UNITS_PER_WORD;
}


/*   A C expression that is 1 if the RTX X is a constant which is a
     valid address.  On most machines, this can be defined as
     `CONSTANT_P (X)', but a few machines are more restrictive in which
     constant addresses are supported.

     `CONSTANT_P' accepts integer-values expressions whose values are
     not explicitly known, such as `symbol_ref', `label_ref', and
     `high' expressions and `const' arithmetic expressions, in addition
     to `const_int' and `const_double' expressions.   */
int
r53_constant_address_p(
rtx x)
{
    return (GET_CODE(x) == LABEL_REF || GET_CODE(x) == SYMBOL_REF || GET_CODE(x) == CONST);
}





/* A C compound statement to output to stdio stream STREAM the
   assembler syntax for an instruction operand X.  X is an RTL
   expression.

   CODE is a value that can be used to specify one of several ways
   of printing the operand.  It is used when identical operands
   must be printed differently depending on the context.  CODE
   comes from the `%' specification that was used to request
   printing of the operand.  If the specification was just `%DIGIT'
   then CODE is 0; if the specification was `%LTR DIGIT' then CODE
   is the ASCII code for LTR.

   If X is a register, this macro should print the register's name.
   The names can be found in an array `reg_names' whose type is
   `char *[]'.  `reg_names' is initialized from `REGISTER_NAMES'.

   When the machine description has a specification `%PUNCT' (a `%'
   followed by a punctuation character), this macro is called with
   a null pointer for X and the punctuation character for CODE.   */
void
r53_print_operand (
FILE *file,			/* file to write to */
rtx x,				/* operand to print */
int letter)			/* %<letter> or 0 */
{
    switch (GET_CODE(x)) {
	case CONST_INT:
	    fprintf(file, "%ld", INTVAL(x));
	    break;
	case REG:
	    fprintf(file, "%s", reg_names[REGNO(x)]);
	    break;
	case SUBREG:
	    fprintf(file, "%s", reg_names[true_regnum(x)]);
	    break;
	case MEM:
	    output_address(XEXP(x, 0));
	    break;
	case SYMBOL_REF:
	case LABEL_REF:
	case CODE_LABEL:
	case CONST:
	    output_addr_const(file, x);
	    break;
	case PLUS:
	    r53_print_operand_address(file, x);
	    break;
	case SIGN_EXTEND:
	    if (GET_MODE(x) == DImode && GET_MODE(XEXP(x, 0)) == SImode && 
		(GET_CODE(XEXP(x, 0)) == REG || GET_CODE(XEXP(x, 0)) == SUBREG)) {
		r53_print_operand(file, XEXP(x, 0), letter);
		break;
	    }
	    /* else fall thru */
	default:
	    abort_with_insn(x, "r53_print_operand: don't know how to print\n");
    }
}

/* A C compound statement to output to stdio stream STREAM the
   assembler syntax for an instruction operand that is a memory
   reference whose address is ADDR.  ADDR is an RTL expression.

   On some machines, the syntax for a symbolic address depends on
   the section that the address refers to.  On these machines,
   define the macro `ENCODE_SECTION_INFO' to store the information
   into the `symbol_ref', and then check for it here.  */
void
r53_print_operand_address(
FILE *file,
rtx addr)
{
    rtx base, disp;
    int rno = 0;
    switch (GET_CODE(addr)) {
	case REG:
	    fprintf(file, "0,%s", reg_names[REGNO(addr)]);
	    break;
	case SUBREG:
	    fprintf(file, "0,%s", reg_names[true_regnum(addr)]);
	    break;
	case SYMBOL_REF:
	case LABEL_REF:
	case CODE_LABEL:
	case CONST:
	    output_addr_const(file, addr);
	    break;
	case MEM:
	    r53_print_operand_address(file, XEXP(addr, 0));
	    break;
	case PLUS:
        case LO_SUM:
	    base = XEXP(addr, 0);
	    disp = XEXP(addr, 1);
	    if (GET_CODE(disp) == CONST_INT)
	      fprintf(file, "%ld", INTVAL(disp));
	    else if (GET_CODE(disp) == SYMBOL_REF || GET_CODE(disp) == CONST) {
	      output_addr_const(file, disp);
	    }
	    else if (GET_CODE(disp) == REG || GET_CODE(disp) == SUBREG) {
	      if (GET_CODE(disp) == REG)
		rno = REGNO(disp);
	      else
		rno = true_regnum(XEXP(disp, 0));
	      if (rno < 0 || rno >= FIRST_PSEUDO_REGISTER)
		abort_with_insn(addr, "r53_print_operand_address: bad index reg\n");
	      fprintf(file, "%s", reg_names[rno]);
	    }
	    else
	      abort_with_insn(addr, "r53_print_operand_address: PLUS right-side\n");
	    if (GET_CODE(base) == REG || GET_CODE(base) == SUBREG) {
	      if (GET_CODE(base) == REG)
		rno = REGNO(base);
	      else
		rno = true_regnum(XEXP(disp, 0));
	      if (rno < 0 || rno >= FIRST_PSEUDO_REGISTER)
		abort_with_insn(addr, "r53_print_operand_address: bad base reg\n");
	      fprintf(file, ",%s", reg_names[rno]);
	    }
	    else
	      abort_with_insn(addr, "r53_print_operand_address: PLUS left-side\n");
	    break;
	default:
	    abort_with_insn(addr, "r53_print_operand_address: Don't know how\n");
    }
}



#if 0
int
const_zero_operand(
rtx op,
enum machine_mode mode)
{
    /*return op == const0_rtx;*/
    return op == CONST0_RTX(mode);
}

int
reg_or_immed_operand(
rtx op,
enum machine_mode mode)
{
    return register_operand(op, mode)
      || immediate_operand(op, mode)
      || op == CONST0_RTX(mode);
}

int
reg_or_int_operand(
rtx op,
enum machine_mode mode)
{
    return register_operand(op, mode)
	|| const_int_operand(op, mode);
}

#endif


void
r53_expand_cstore (rtx operands[], enum machine_mode mode)
{
  enum rtx_code code = GET_CODE(operands[1]);
  fprintf(stderr, "**********in expand_cstore\n");
#if 0
  /*
   * Left operand must be in register.
   */
  if (!register_operand(operands[2], mode) && can_create_pseudo_p())
    operands[2] = force_reg(mode, operands[2]);
  /*
   * Right operand can be register or small immediate including zero.
   */
  if (const_int_operand(operands[3], mode) && CONST_OK_FOR_LETTER_P(INTVAL(operands[3]), 'I'))
    ; /* ok */
  else if (!register_operand(operands[3], mode) && can_create_pseudo_p())
    operands[3] = force_reg(mode, operands[3]);
#endif

  switch (mode) {
  case SImode:
  case DImode:
    emit_move_insn(operands[0], gen_rtx_fmt_ee(code, mode, operands[2], operands[3]));
    break;
  case SFmode:
    emit_move_insn(operands[0], gen_rtx_ZERO_EXTRACT(SImode, gen_rtx_fmt_ee(code, mode, operands[2], operands[3]), GEN_INT(1), GEN_INT(0)));
    break;
  case DFmode:
    emit_move_insn(operands[0], gen_rtx_ZERO_EXTRACT(DImode, gen_rtx_fmt_ee(code, mode, operands[2], operands[3]), GEN_INT(1), GEN_INT(0)));
    break;
  default:
    abort();
  }
}


rtx
r53_expand_movcc(rtx cmp, enum machine_mode mode)
{
  enum rtx_code code = GET_CODE (cmp);
  rtx op0 = XEXP (cmp, 0);
  rtx op1 = XEXP (cmp, 1);
  rtx tmp, x;
  enum machine_mode flag_mode = mode;

  if (mode == SFmode) flag_mode = SImode;
  if (mode == DFmode) flag_mode = DImode;
  tmp = gen_reg_rtx(flag_mode);

  fprintf(stderr, "********** in expand_movcc\n");

  if (GET_MODE_CLASS(GET_MODE(op0)) == MODE_INT)
    x = emit_insn(gen_rtx_SET(flag_mode, tmp, gen_rtx_fmt_ee(code, GET_MODE(op0), op0, op1)));
  else
    x = emit_insn(gen_rtx_SET(flag_mode, tmp, gen_rtx_ZERO_EXTRACT(flag_mode, gen_rtx_fmt_ee(code, GET_MODE(op0), op0, op1), GEN_INT(1), GEN_INT(0))));
  return gen_rtx_fmt_ee(NE, flag_mode, tmp, CONST0_RTX(flag_mode)); 
}

void
r53_expand_vcond(rtx operands[])
{

  /*
  if (!register_operand(operands[1], mode) && can_create_pseudo_p())
    operands[1] = force_reg(mode, operands[1]);
  if (!register_operand(operands[2], mode) && can_create_pseudo_p())
    operands[2] = force_reg(mode, operands[2]);
  if (!register_operand(operands[4], mode) && can_create_pseudo_p())
    operands[4] = force_reg(mode, operands[4]);
  if (!reg_or_imm_operand(operands[5], mode) && can_create_pseudo_p())
    operands[5] = force_reg(mode, operands[5]);
  */

  enum machine_mode data_mode = GET_MODE(operands[0]);
  rtx pred = gen_reg_rtx(PVmode);
  rtx true_move  = gen_rtx_SET(data_mode, operands[0], operands[1]);
  rtx false_move = gen_rtx_SET(data_mode, operands[0], operands[2]);

  fprintf(stderr, "********** in expand_vcond\n");

  emit_insn(gen_rtx_fmt_ee(COND_EXEC, VOIDmode, gen_rtx_fmt_ee(NE, PVmode, pred, const0_rtx), true_move));
  emit_insn(gen_rtx_fmt_ee(COND_EXEC, VOIDmode, gen_rtx_fmt_ee(EQ, PVmode, pred, const0_rtx), false_move));
}



int
r53_expand_cbranch(rtx operands[], enum machine_mode mode ATTRIBUTE_UNUSED)
{
  if (!TARGET_COND_EXEC)
    return 0;

  /*
  if (!register_operand(operands[1], mode) && can_create_pseudo_p())
    operands[1] = force_reg(mode, operands[1]);
  if (!register_operand(operands[2], mode) && can_create_pseudo_p())
    operands[2] = force_reg(mode, operands[2]);
  */
  /*  if (GET_MODE_CLASS(mode) == MODE_FLOAT) {*/
  {
    rtx tmp = gen_reg_rtx(BImode);
    emit_insn(gen_rtx_SET(BImode, tmp, gen_rtx_fmt_ee(GET_CODE(operands[0]), BImode, operands[1], operands[2])));
    operands[0] = gen_rtx_fmt_ee(NE, BImode, tmp, const0_rtx);
    operands[1] = tmp;
    operands[2] = const0_rtx;
    return 0;
  }
  /*    operands[0] = gen_rtx_fmt_ee(GET_CODE(operands[0]), BImode, operands[1], operands[2]); */
  return 0;
}



void
r53_encode_section_info (
tree decl)
{
    if (TREE_CODE (decl) == FUNCTION_DECL)
	SYMBOL_REF_FLAG (XEXP (DECL_RTL (decl), 0)) = 1;
}

static void
gen_const28(enum machine_mode mode, rtx result, long long value)
{
  long long low = ((value & 0x3ff) ^ 0x200) - 0x200;
  long long high = value - low;
  rtx t1 = can_create_pseudo_p() ? gen_reg_rtx(mode) : result;
  emit_insn(gen_rtx_SET(mode, t1, GEN_INT(high)));
  emit_insn((mode==DImode?gen_adddi3:gen_addsi3)(result, t1, GEN_INT(low)));
}

void
r53_expand_move(rtx operands[])
{
  enum machine_mode mode = GET_MODE(operands[0]);
  /*
  if (GET_CODE(operands[1]) == CONST_INT) {
    fprintf(stderr, "----- reload_in_progress=%d\n", reload_in_progress);
    debug_rtx(operands[0]);
    debug_rtx(operands[1]);
  }
  */
#if 1
  /* Handle the case where reload calls us with an invalid address.  */
  if (reload_in_progress && mode == Pmode && (!general_operand(operands[1], mode) || !nonimmediate_operand(operands[0], mode))) {
    emit_insn (gen_rtx_SET (mode, operands[0], operands[1]));
    return;
  }
#endif

  if (can_create_pseudo_p() && GET_CODE(operands[0]) == MEM && !register_operand(operands[1], mode))
    operands[1] = force_reg (mode, operands[1]);

  if (GET_MODE_CLASS(GET_MODE(operands[0])) == MODE_INT && GET_CODE(operands[1]) == CONST_INT) {
    int64_t value = INTVAL(operands[1]);
    unsigned stack[3];
    int top = 0;
    rtx reg = operands[0];
    rtx x;

    while ((value >> 27) != 0LL && (value >> 27) != -1LL) {
      stack[top++] = value & 0xFFFll;
      value >>= 12;
    }
#define DEBUG  0
    if (DEBUG && top > 0) {
      fprintf(stderr, "***** Big constant %d parts:\n", top);
      debug_rtx(operands[0]);
      debug_rtx(operands[1]);
      fprintf(stderr, "---->\n");
    }
    if (top > 0 && can_create_pseudo_p())
      reg = gen_reg_rtx(mode);
    if (CONST_OK_FOR_LETTER_P(value, 'L') || CONST_OK_FOR_LETTER_P(value, 'P')) {
      x=emit_insn(gen_rtx_SET(mode, reg, GEN_INT(value)));
      if (DEBUG && top > 0) debug_rtx(x);
    }
    else {
      long long low = ((value & 0x3ff) ^ 0x200) - 0x200;
      long long high = value - low;
      rtx t1 = can_create_pseudo_p() ? gen_reg_rtx(mode) : operands[0];
      x=emit_insn(gen_rtx_SET(mode, t1, GEN_INT(high)));
      if (DEBUG && top > 0) debug_rtx(x);
      x=emit_insn((mode==DImode?gen_adddi3:gen_addsi3)(reg, t1, GEN_INT(low)));
      if (DEBUG && top > 0) debug_rtx(x);
    }
    while (top > 0) {
      rtx into = (top > 1 && can_create_pseudo_p()) ? gen_reg_rtx(mode) : operands[0];
      x=emit_insn((mode==DImode?gen_enter_lowdi3:gen_enter_lowsi3)(into, reg, GEN_INT(stack[top-1])));
      if (DEBUG && top > 0) debug_rtx(x);
      reg = into;
      top--;
    }
    return;
  }
  if (GET_CODE(operands[1]) == LABEL_REF || GET_CODE(operands[1]) == SYMBOL_REF || GET_CODE(operands[1]) == CONST) {
    rtx t1 = operands[0];
    if (can_create_pseudo_p())
      t1 = gen_reg_rtx(mode);
    emit_insn (gen_rtx_SET (mode, t1, gen_rtx_HIGH (mode, operands[1])));
    emit_insn (gen_rtx_SET (mode, operands[0], gen_rtx_LO_SUM (mode, t1, operands[1])));
    return;
  }
  /* Everything else is just normal move */
  emit_insn (gen_rtx_SET (mode, operands[0], operands[1]));
}

int
r53_expand_vec_init(rtx operands[])
{
  fprintf(stderr, "********** in expand_vec_init\n");
  rtx scalar = XVECEXP(operands[1], 0, 0);
  int i;
  for (i=1; i<GET_MODE_NUNITS(GET_MODE(operands[1])); i++) {
    if (XVECEXP(operands[1], 0, i) != scalar)
      return 0;
  }
  operands[1] = scalar; 
  return 1;
}




static enum reg_class
r53_secondary_reload(bool in_p, rtx x, enum reg_class rclass, enum machine_mode mode, secondary_reload_info *sri)
{
  if (rclass != PREDICATE_REGS)
    return NO_REGS;

  fprintf(stderr, "***** secondary_reload with PREDICATE_REGS\n");
  debug_rtx(x);

  int regno = true_regnum(x);
  if (0 <= regno && R53_GENERAL_REGS+R53_VECTOR_REGS <= regno && regno < FIRST_PSEUDO_REGISTER) {
    sri->icode = (in_p ? reload_in_optab[mode] : reload_out_optab[mode]);
    return GENERAL_REGS;
  }

  return NO_REGS;
}



void
r53_expand_call(
rtx operands[],
int returning_value)
{
    int k = returning_value ? 1 : 0;

    /* Direct function calls need special treatment.  */
    if (GET_CODE (XEXP (operands[k], 0)) == SYMBOL_REF)
    {
	rtx sym = XEXP (operands[k], 0);
	rtx target = gen_reg_rtx (Pmode);
	emit_move_insn (target, sym);
	sym = target;
	operands[k] = gen_rtx_MEM (SImode, sym);
    }
}









int
r53_legitimate_address_p(
enum machine_mode mode ATTRIBUTE_UNUSED,
rtx x,
int strict)
{
  /* Indirect through plain register is same as zero-displacement. */
  if (GET_CODE(x) == REG || GET_CODE(x) == SUBREG) {
    if (GET_CODE(x) == SUBREG) {
      x = XEXP(x, 0);
      if (GET_CODE(x) != REG)
	return 0;
    }
    if (strict)
      return REG_OK_FOR_BASE_STRICT_P(x);
    else
      return REG_OK_FOR_BASE_NONSTRICT_P(x);
  }

  /* General displacement-mode addressing. */			
  if (GET_CODE(x) == PLUS || GET_CODE(x) == LO_SUM) {
    rtx base = XEXP(x, 0);
    rtx disp = XEXP(x, 1);

    if (GET_CODE(base) != REG && GET_CODE(base) != SUBREG)
      return 0;
    if (GET_CODE(base) == SUBREG) {
      base = XEXP(base, 0);
      if (GET_CODE(base) != REG)
	return 0;
    }
    /* Base register valid? */
    if ((strict && !REG_OK_FOR_BASE_STRICT_P(base)) || (!strict && !REG_OK_FOR_BASE_NONSTRICT_P(base)))
      return 0;

    /* Offset must fit within 'M' bits. */
    if (GET_CODE(disp) == CONST_INT)
      return CONST_OK_FOR_LETTER_P(INTVAL(disp), 'M');

    if (GET_CODE(disp) == LABEL_REF || GET_CODE(disp) == SYMBOL_REF || GET_CODE(disp) == CONST)
      return GET_CODE(x) == LO_SUM;

    /* Must be reg+reg */
    if (!TARGET_INDEX)
      return 0;

    if (GET_CODE(disp) != REG && GET_CODE(disp) != SUBREG)
      return 0;
    if (GET_CODE(disp) == SUBREG) {
      disp = XEXP(disp, 0);
      if (GET_CODE(disp) != REG)
	return 0;
    }
    /* Index register valid? */
    if (strict)
      return REG_OK_FOR_INDEX_STRICT_P(disp);
    else
      return REG_OK_FOR_INDEX_NONSTRICT_P(disp);
  }
  /* Anything else is not legitimate. */
  return 0;
}

rtx
r53_legitimize_reload_address(rtx x, enum machine_mode mode, int opnum, int typ, int ind_levels, int *win)
{
  *win = 0;
  if (GET_CODE(x) != PLUS)
    return x;

  rtx op0 = XEXP(x, 0);
  rtx op1 = XEXP(x, 1);

#if 0
  if (REG_P(op0) && REGNO(op0) >= FIRST_PSEUDO_REGISTER) {
    push_reload(XEXP(x, 0), NULL_RTX, &XEXP(x, 0), 0, BASE_REG_CLASS, GET_MODE(x), VOIDmode, 0, 0, opnum, (enum reload_type)typ);
    *win = 1;
  }
  if (REG_P(op1) && REGNO(op1) >= FIRST_PSEUDO_REGISTER) {
    push_reload(XEXP(x, 1), NULL_RTX, &XEXP(x, 1), 0, BASE_REG_CLASS, GET_MODE(x), VOIDmode, 0, 0, opnum, (enum reload_type)typ);
    *win = 1;
  }
#endif
  return x;
}


#if 0
/*   Instruction to jump through a dispatch table, including bounds
     checking.  This instruction takes five operands:

     0. The index to dispatch on, which has mode `SImode'.

     1. The lower bound for indices in the table, an integer constant.

     2. The total range of indices in the table--the largest index
     minus the smallest one (both inclusive).

     3. A label that precedes the table itself.

     4. A label to jump to if the index has a value outside the
     bounds.  (If the machine-description macro
     `CASE_DROPS_THROUGH' is defined, then an out-of-bounds index
     drops through to the code following the jump table instead of
     jumping to this label.  In that case, this label is not
     actually used by the `casesi' instruction, but it is always
     provided as an operand.)

     The table is a `addr_vec' or `addr_diff_vec' inside of a
     `jump_insn'.  The number of elements in the table is one plus the
     difference between the upper bound and the lower bound.
*/
void
r53_expand_casesi(
rtx operands[])
{
    rtx index;
    rtx cases = GEN_INT (INTVAL (operands[2]) + 1);
    if (operands[1] != const0_rtx) {
	index = gen_reg_rtx(SImode);
	emit_insn (gen_addsi3 (index, operands[0], GEN_INT(- INTVAL(operands[1]))));
    }
    else
	index = force_reg(SImode, operands[0]);
    emit_jump_insn (gen_jcasesi5 (index, cases, GEN_INT (2*INTVAL (cases)+4), index, operands[3]));
}
#endif

void
r53_expand_tablejump(
     rtx operands[])
{
  rtx tmp;
  tmp = gen_rtx_LABEL_REF (Pmode, operands[1]);
  /*tmp = gen_rtx_PLUS (Pmode, gen_rtx_PLUS(Pmode, operands[0], operands[0]), tmp);*/
  /*operands[0] = memory_address (Pmode, tmp);*/
  operands[0] = memory_address (Pmode, operands[0]);
}

/*
 * Table elements should be the BYTE difference between
 * label after jump instruction and destination.
 */
void
r53_asm_output_addr_diff_elt(
FILE* stream,
int value,
int rel)
{
  /*    fprintf(stream, "\t.short\t(.L%d-.L%d+4)/2\n", value, rel);*/
    fprintf(stream, "\t.short\t(.L%d-.L%d)/2\n", value, rel);
}

void
r53_asm_output_align(
FILE* stream,
int power)
{
    fprintf(stream, "\t.align %d\n", power);
}

static void
r53_globalize_label (FILE *stream, const char *name)
{
  fputs (".global ", stream);
  assemble_name (stream, name);
  fputc (';',stream);
  fputc ('\n',stream);
}


static bool
r53_rtx_costs (rtx x, int code, int outer_code, int *total, bool speed)
{
  rtx op0, op1;
  HOST_WIDE_INT imm;
  switch (code) {
  case CONST_INT:
    switch (outer_code) {
    case SET:
      *total = CONST_OK_FOR_LETTER_P(INTVAL(x), 'L') ? 0 : COSTS_N_INSNS(1);
      break;
    case PLUS:
      *total = CONST_OK_FOR_LETTER_P(INTVAL(x), 'M') ? 0 : COSTS_N_INSNS(1);
      break;
    default:
      *total = CONST_OK_FOR_LETTER_P(INTVAL(x), 'J') ? 0 : COSTS_N_INSNS(1);
    }
    return true;

  case CONST_DOUBLE:
    *total = COSTS_N_INSNS (2);
    return true;

  case LABEL_REF:
  case SYMBOL_REF:
  case CONST:
    *total = (outer_code == LO_SUM) ? 0 : COSTS_N_INSNS(2); /* high+addi */
    return true;

  case HIGH:
    *total = COSTS_N_INSNS (1);
    return true;

  case LO_SUM:
    *total = (outer_code == MEM) ? 0 : COSTS_N_INSNS(1);
    return true;

  case DIV:
  case UDIV:
  case MOD:
  case UMOD:
    /* We make divide expensive, so that divide-by-constant will be
       optimized to a multiply.  */
    *total = COSTS_N_INSNS (60);
    return true;

  default:
    return false;
  }
}


void
r53_ifcvt_modify_tests(void* foo, rtx* true_expr, rtx* false_expr)
{
  /*
    fprintf(stderr, "******* ifcvt_modify_test()\n");
    debug_rtx(true_expr);
    debug_rtx(false_expr);
  */
}


void
r53_trampoline_init(rtx m_tramp, tree fndecl, rtx static_chain) {
  abort();
}



#undef TARGET_PROMOTE_PROTOTYPES
#define TARGET_PROMOTE_PROTOTYPES hook_bool_const_tree_true

#undef TARGET_FUNCTION_VALUE
#define TARGET_FUNCTION_VALUE r53_function_value

#undef TARGET_SETUP_INCOMING_VARARGS
#define TARGET_SETUP_INCOMING_VARARGS r53_setup_incoming_varargs

#undef TARGET_ASM_GLOBALIZE_LABEL
#define TARGET_ASM_GLOBALIZE_LABEL r53_globalize_label 

#undef TARGET_RTX_COSTS
#define TARGET_RTX_COSTS r53_rtx_costs

#undef TARGET_ADDRESS_COST
#define TARGET_ADDRESS_COST  hook_int_rtx_bool_0

#undef TARGET_VECTOR_MODE_SUPPORTED_P
#define TARGET_VECTOR_MODE_SUPPORTED_P   r53_vector_mode_supported_p

#undef TARGET_LEGITIMATE_ADDRESS_P
#define TARGET_LEGITIMATE_ADDRESS_P   r53_legitimate_address_p

#undef LEGITIMATE_CONSTANT_P
#define LEGITIMATE_CONSTANT_P   r53_legitimate_constant_p


#undef TARGET_TRAMPOLINE_INIT
#define TARGET_TRAMPOLINE_INIT   r53_trampoline_init

/*
#undef TARGET_SECONDARY_RELOAD
#define TARGET_SECONDARY_RELOAD   r53_secondary_reload
*/

/* No need
#undef TARGET_HAVE_CONDITIONAL_EXECUTION
#define TARGET_HAVE_CONDITIONAL_EXECUTION   hook_bool_const_tree_true
*/

struct gcc_target targetm = TARGET_INITIALIZER;

