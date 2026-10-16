/* r53.h - Target machine macros for GNU compiler.
   Copyright 2026 by Peter Hsu
*/


#ifndef _R53_CONFIG
#define _R53_CONFIG


/* Run-time compilation parameters selecting different hardware subsets.  */

extern int target_flags;

/* Predefinition in the preprocessor for this target machine */
#ifndef TARGET_CPU_CPP_BUILTINS
#define TARGET_CPU_CPP_BUILTINS()		\
  do						\
    {						\
      builtin_define_std ("r53");		\
      builtin_define_std ("R53");		\
      builtin_define_std ("GNU_R53");		\
      builtin_define ("__R53_ABI_version=1");	\
      builtin_assert ("cpu=r53");		\
      builtin_assert ("machine=r53");		\
    }						\
  while (0)
#endif


/*   Define this macro if debugging can be performed even without a
     frame pointer.  If this macro is defined, GCC will turn on the
     `-fomit-frame-pointer' option whenever `-O' is specified.   */
#define CAN_DEBUG_WITHOUT_FP

#define Pmode   (TARGET_64BIT ? DImode : SImode)

/* store-condition-codes instructions store 0 for false
   This is the value stored for true.  */
#define STORE_FLAG_VALUE   1



/* Define this if pushing a word on the stack
   makes the stack pointer a smaller address.  */
#define STACK_GROWS_DOWNWARD  1

/* We define a dummy ARGP register; the parameters start at offset 0 from
   it. */
#define FIRST_PARM_OFFSET(DECL)   0

/* Register to use for pushing function arguments.  */
#define STACK_POINTER_REGNUM   0

/* Return address register */
#define RETURN_ADDR_REGNUM   (R53_GENERAL_REGS-1)

/* Base register for access to local variables of the function.  */
#define FRAME_POINTER_REGNUM   (R53_GENERAL_REGS-2)

/* A dummy register that will be eliminated to either FP or SP.  */
#define ARG_POINTER_REGNUM   (R53_GENERAL_REGS-2)

#define STATIC_CHAIN_REGNUM   (R53_GENERAL_REGS-3)

#if 0
/* `PIC_OFFSET_TABLE_REGNUM'
     The register number of the register used to address a table of
     static data addresses in memory.  In some cases this register is
     defined by a processor's "application binary interface" (ABI).
     When this macro is defined, RTL is generated for this register
     once, as with the stack pointer and frame pointer registers.  If
     this macro is not defined, it is up to the machine-dependent files
     to allocate such a register (if necessary). */
#define PIC_OFFSET_TABLE_REGNUM (REG_P5)

#define FDPIC_FPTR_REGNO REG_P1
#define FDPIC_REGNO REG_P3
#define OUR_FDPIC_REG	get_hard_reg_initial_val (SImode, FDPIC_REGNO)

#endif
	  
/* Define this if the maximum size of all the outgoing args is to be
   accumulated and pushed during the prologue.  The amount can be
   found in the variable crtl->outgoing_args_size. */ 
#define ACCUMULATE_OUTGOING_ARGS   1

/*#define DATA_ALIGNMENT(TYPE, BASIC-ALIGN) for arrays.. */

/* If defined, a C expression to compute the alignment for a local
   variable.  TYPE is the data type, and ALIGN is the alignment that
   the object would ordinarily have.  The value of this macro is used
   instead of that alignment to align the object.

   If this macro is not defined, then ALIGN is used.

   One use of this macro is to increase alignment of medium-size
   data to make it all fit in fewer cache lines.  */

#define DATA_ALIGNMENT(EXP, ALIGN)                 r53_data_alignment((EXP), (ALIGN))
#define LOCAL_ALIGNMENT(TYPE, ALIGN)              r53_local_alignment((TYPE), VOIDmode, (ALIGN))
#define STACK_SLOT_ALIGNMENT(TYPE, MODE, ALIGN)   r53_local_alignment((TYPE), (MODE), (ALIGN))

/* Make strings word-aligned so strcpy from constants will be faster.  */
#define CONSTANT_ALIGNMENT(EXP, ALIGN)            r53_constant_alignment((EXP), (ALIGN))


#if 0

#define TRAMPOLINE_SIZE (TARGET_FDPIC ? 30 : 18)
#define TRAMPOLINE_TEMPLATE(FILE)                                       \
  if (TARGET_FDPIC)							\
    {									\
      fprintf(FILE, "\t.dd\t0x00000000\n"); /* 0 */			\
      fprintf(FILE, "\t.dd\t0x00000000\n"); /* 0 */			\
      fprintf(FILE, "\t.dd\t0x0000e109\n"); /* p1.l = fn low */		\
      fprintf(FILE, "\t.dd\t0x0000e149\n"); /* p1.h = fn high */	\
      fprintf(FILE, "\t.dd\t0x0000e10a\n"); /* p2.l = sc low */		\
      fprintf(FILE, "\t.dd\t0x0000e14a\n"); /* p2.h = sc high */	\
      fprintf(FILE, "\t.dw\t0xac4b\n"); /* p3 = [p1 + 4] */		\
      fprintf(FILE, "\t.dw\t0x9149\n"); /* p1 = [p1] */			\
      fprintf(FILE, "\t.dw\t0x0051\n"); /* jump (p1)*/			\
    }									\
  else									\
    {									\
      fprintf(FILE, "\t.dd\t0x0000e109\n"); /* p1.l = fn low */		\
      fprintf(FILE, "\t.dd\t0x0000e149\n"); /* p1.h = fn high */	\
      fprintf(FILE, "\t.dd\t0x0000e10a\n"); /* p2.l = sc low */		\
      fprintf(FILE, "\t.dd\t0x0000e14a\n"); /* p2.h = sc high */	\
      fprintf(FILE, "\t.dw\t0x0051\n"); /* jump (p1)*/			\
    }

#define INITIALIZE_TRAMPOLINE(TRAMP, FNADDR, CXT) \
  initialize_trampoline (TRAMP, FNADDR, CXT)

#endif

/* Definitions for register eliminations.

   This is an array of structures.  Each structure initializes one pair
   of eliminable registers.  The "from" register number is given first,
   followed by "to".  Eliminations of the same "from" register are listed
   in order of preference.

   There are two registers that can always be eliminated on the i386.
   The frame pointer and the arg pointer can be replaced by either the
   hard frame pointer or to the stack pointer, depending upon the
   circumstances.  The hard frame pointer is not used before reload and
   so it is not eligible for elimination.  */

#define ELIMINABLE_REGS				\
{{ ARG_POINTER_REGNUM, STACK_POINTER_REGNUM},	\
 { ARG_POINTER_REGNUM, FRAME_POINTER_REGNUM},	\
 { FRAME_POINTER_REGNUM, STACK_POINTER_REGNUM}}	\

/* Define the offset between two registers, one to be eliminated, and the other
   its replacement, at the start of a routine.  */

#define INITIAL_ELIMINATION_OFFSET(FROM, TO, OFFSET)   ((OFFSET) = r53_initial_elimination_offset((FROM), (TO)))


/* This processor has
   64 general registers
   64 vector registers
*/

#define FIRST_PSEUDO_REGISTER  (R53_GENERAL_REGS + R53_VECTOR_REGS + R53_PREDICATE_REGS)

/*   A C initializer containing the assembler's names for the machine
     registers, each one as a C string constant.  This is what
     translates register numbers in the compiler into assembler
     language.   */
#define REGISTER_NAMES   { \
  "sp",  "a1",  "a2",  "a3",  "a4",  "a5",  "a6",  "a7",	\
  "s0",  "s1",  "s2",  "s3",  "s4",  "s5",  "s6",  "ra",	\
  "t0",  "t1",  "t2",  "t3",  "t4",  "t5",  "t6",  "t7",	\
  "t8",  "t9",  "t10", "t11", "t12", "t13", "t14", "t15",	\
  "t16", "t17", "t18", "t19", "t20", "t21", "t22", "t23",	\
  "t24", "t25", "t26", "t27", "t28", "t29", "t30", "t31",	\
  "t32", "t33", "t34", "t35", "t36", "t37", "t38", "t39",	\
  "t40", "t41", "t42", "t43", "t44", "t45", "t46", "t47",	\
}

/* 1 for registers that have pervasive standard uses
   and are not available for the register allocator.  */

/* Default is just $sp.  See r53.c */
#define FIXED_REGISTERS   { \
  1, 0, 0, 0, 0, 0, 0, 0,				/* sp,a1..a7 */	\
  0, 0, 0, 0, 0, 0, 0, 0,				/* s0..s7 */	\
  0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0,	/* t0 ..t15 */	\
  0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0,	/* t16..t31 */	\
  0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0,	/* t32..t47 */	\
}

#define CALL_USED_REGISTERS  {						\
  1, 1, 1, 1, 1, 1, 1, 1,				/* a0..a7 */   	\
  1, 1, 1, 1, 1, 1, 1, 1,				/* s0..s7 */	\
  0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0,	/* t0 ..t15 */	\
  0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0,	/* t16..t31 */	\
  0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0,	/* t32..t47 */	\
}

/* Order in which to allocate registers.  Each register must be
   listed once, even those in FIXED_REGISTERS.  List frame pointer
   late and fixed registers last.  Note that, in general, we prefer
   registers listed in CALL_USED_REGISTERS, keeping the others
   available for storage of persistent values. */

#define REG_ALLOC_ORDER  { \
  15, 7, 14, 6, 13, 5, 12, 4, 11, 3, 10, 2, 9, 1, 8,			\
  63, 62, 61, 60, 59, 58, 57, 56, 55, 54, 53, 52, 51, 50, 49, 48,	\
  47, 46, 45, 44, 43, 42, 41, 40, 39, 38, 37, 36, 35, 34, 33, 32,	\
  31, 30, 29, 28, 27, 26, 25, 24, 23, 22, 21, 20, 19, 18, 17, 16,	\
  0, }

/* Define the classes of registers for register constraints in the
   machine description.  Also define ranges of constants.

   One of the classes must always be named ALL_REGS and include all hard regs.
   If there is more than one class, another class must be named NO_REGS
   and contain no registers.

   The name GENERAL_REGS must be the name of a class (or an alias for
   another name such as ALL_REGS).  This is the class of registers
   that is allowed by "g" or "r" in a register constraint.
   Also, registers outside this class are allocated only when
   instructions express preferences for them.

   The classes must be numbered in nondecreasing order; that is,
   a larger-numbered class must never be contained completely
   in a smaller-numbered class.

   For any two classes, it is very desirable that there be another
   class that represents their union. */


enum reg_class {
  NO_REGS,		/* required by gcc			*/
  A_REGS,		/* a0-a7		('a') */
  S_REGS,		/* s0-s7		('s') */
  GENERAL_REGS,		/* a0-a7,s0-s7		('g') */
  U_REGS,		/* t0-t47		      */
  T_REGS,		/* a0-a7,s0-s7,t0-t47	('t') */
  ALL_REGS,		/* required by gcc			*/
  LIM_REG_CLASSES	/* end marker, not a class		*/
};

#define N_REG_CLASSES ((int)LIM_REG_CLASSES)

#define REG_CLASS_NAMES   {						\
  "NO_REGS",		/* required by gcc			*/	\
  "A_REGS",		/* a0-a7		('a') */
  "S_REGS",		/* s0-s7		('s') */
  "GENERAL_REGS",	/* a0-a7,s0-s7		('g') */
  "U_REGS",		/* t0-t47		      */
  "T_REGS",		/* a0-a7,s0-s7,t0-t47	('t') */
  "ALL_REGS",		/* required by gcc			*/	\
}
/*   An initializer containing the names of the register classes as C
     string constants.  These names are used in writing some of the
     debugging dumps.   */

#define REG_CLASS_CONTENTS   {					   \
  { 0x00000000, 0x00000000, },		/* required by gcc	*/ \
  { 0x000000ff, 0x00000000, },		/* A_REGS		*/ \
  { 0x0000ff00, 0x00000000, },		/* S_REGS		*/ \
  { 0x0000ffff, 0x00000000, },		/* GENERAL_REGS		*/ \
  { 0xffff0000, 0xffffffff, },		/* U_REGS		*/ \
  { 0xffffffff, 0xffffffff, },		/* T_REGS		*/ \
  { 0xffffffff, 0xffffffff, },		/* ALL_REGS		*/ \
}

/*   A C expression whose value is a register class containing hard
     register REGNO.  In general there is more than one such class;
     choose a class which is "minimal", meaning that no smaller class
     also contains the register.   */
#define REGNO_REG_CLASS(REGNO)   r53_regno_reg_class(REGNO)
/*   A C expression which defines the machine-dependent operand
     constraint letters for register classes.  If CHAR is such a
     letter, the value should be the register class corresponding to
     it.  Otherwise, the value should be `NO_REGS'.  The register
     letter `r', corresponding to class `GENERAL_REGS', will not be
     passed to this macro; you do not need to handle it.   */
#define REG_CLASS_FROM_LETTER(CHAR)   r53_reg_class_from_letter(CHAR)

/*   A macro whose definition is the name of the class to which a valid
     base register must belong.  A base register is one used in an
     address which is the register value plus a displacement.   */
#define BASE_REG_CLASS   A_REGS

/*   A macro whose definition is the name of the class to which a valid
     index register must belong.  An index register is one used in an
     address where its value is either multiplied by a scale factor or
     added to another register (as well as added to a displacement).   */
#define INDEX_REG_CLASS   S_REGS

/*   A C expression which is nonzero if register number NUM is suitable
     for use as a base register in operand addresses.  It may be either
     a suitable hard register or a pseudo register that has been
     allocated such a hard register.   */
#define REGNO_OK_FOR_BASE_P(num)   ((0 <= (num) && (num) <=  7) || (0 <= reg_renumber[num] && reg_renumber[num] <= 7 ))
#define REGNO_OK_FOR_INDEX_P(num)  ((8 <= (num) && (num) <= 15) || (8 <= reg_renumber[num] && reg_renumber[num] < 15))


/*   A C expression that is nonzero if X (assumed to be a `reg' RTX) is
     valid for use as a base register.  For hard registers, it should
     always accept those which the hardware permits and reject the
     others.  Whether the macro accepts or rejects pseudo registers
     must be controlled by `REG_OK_STRICT' as described above.  This
     usually requires two variant definitions, of which `REG_OK_STRICT'
     controls the one actually used.   */
#define REG_OK_FOR_BASE_STRICT_P(x)       (REGNO_OK_FOR_BASE_P(REGNO(x)))
#define REG_OK_FOR_BASE_NONSTRICT_P(x)   ((REGNO_OK_FOR_BASE_P(REGNO(x))) || (REGNO(x) >= FIRST_PSEUDO_REGISTER))

#define REG_OK_FOR_INDEX_STRICT_P(x)      (REGNO_OK_FOR_INDEX_P(REGNO(x)))
#define REG_OK_FOR_INDEX_NONSTRICT_P(x)  ((REGNO_OK_FOR_INDEX_P(REGNO(x))) || (REGNO(x) >= FIRST_PSEUDO_REGISTER))

#ifdef REG_OK_STRICT
#define REG_OK_FOR_BASE_P(x)     REG_OK_FOR_BASE_STRICT_P(x)
#define REG_OK_FOR_INDEX_P(x)    REG_OK_FOR_INDEX_STRICT_P(x)
#else
#define REG_OK_FOR_BASE_P(x)     REG_OK_FOR_BASE_NONSTRICT_P(x)
#define REG_OK_FOR_INDEX_P(x)    REG_OK_FOR_INDEX_NONSTRICT_P(x)
#endif

/*   A C expression for the number of consecutive hard registers,
     starting at register number REGNO, required to hold a value of mode
     MODE.  */
#define HARD_REGNO_NREGS(REGNO, MODE)  r53_hard_regno_nregs((REGNO), (MODE))

/*   A C expression that is nonzero if it is permissible to store a
     value of mode MODE in hard register number REGNO (or in several
     registers starting with that one).  */
#define HARD_REGNO_MODE_OK(REGNO, MODE)   r53_hard_regno_mode_ok((REGNO), (MODE))

/*   A C expression for the maximum number of consecutive registers of
     class CLASS needed to hold a value of mode MODE.

     This is closely related to the macro `HARD_REGNO_NREGS'.  In fact,
     the value of the macro `CLASS_MAX_NREGS (CLASS, MODE)' should be
     the maximum value of `HARD_REGNO_NREGS (REGNO, MODE)' for all
     REGNO values in the class CLASS.

     This macro helps control the handling of multiple-word values in
     the reload pass.   */
#define CLASS_MAX_NREGS(CLASS, MODE)  HARD_REGNO_NREGS(0, (MODE))

/* A C expression that is nonzero if it is desirable to choose
   register allocation so as to avoid move instructions between a
   value of mode MODE1 and a value of mode MODE2.

   If `HARD_REGNO_MODE_OK (R, MODE1)' and `HARD_REGNO_MODE_OK (R,
   MODE2)' are ever different for any R, then `MODES_TIEABLE_P (MODE1,
   MODE2)' must be zero. */
#define MODES_TIEABLE_P(MODE1, MODE2)   r53_modes_tieable_p((MODE1), (MODE2))

/*   A C expression that places additional restrictions on the register
     class to use when it is necessary to copy value X into a register
     in class CLASS.  The value is a register class; perhaps CLASS, or
     perhaps another, smaller class.  On many machines, the following
     definition is safe:

          #define PREFERRED_RELOAD_CLASS(X,CLASS) CLASS

     Sometimes returning a more restrictive class makes better code.
     For example, on the 68000, when X is an integer constant that is
     in range for a `moveq' instruction, the value of this macro is
     always `DATA_REGS' as long as CLASS includes the data registers.
     Requiring a data register guarantees that a `moveq' will be used.

     If X is a `const_double', by returning `NO_REGS' you can force X
     into a memory constant.  This is useful on certain machines where
     immediate floating values cannot be loaded into certain kinds of
     registers.   */
#define PREFERRED_RELOAD_CLASS(X, CLASS)   CLASS



/* Function Calling Conventions. */

/*   A C type for declaring a variable that is used as the first
     argument of `FUNCTION_ARG' and other related values.  For some
     target machines, the type `int' suffices and can hold the number
     of bytes of argument so far.

     There is no need to record in `CUMULATIVE_ARGS' anything about the
     arguments that have been passed on the stack.  The compiler has
     other variables to keep track of that.  For target machines on
     which all arguments are passed on the stack, there is no need to
     store anything in `CUMULATIVE_ARGS'; however, the data structure
     must exist and should not be empty, so use `int'.   */
#define CUMULATIVE_ARGS   int

/*   A C statement (sans semicolon) for initializing the variable CUM
     for the state at the beginning of the argument list.  The variable
     has type `CUMULATIVE_ARGS'.  The value of FNTYPE is the tree node
     for the data type of the function which will receive the args, or 0
     if the args are to a compiler support library function.  The value
     of INDIRECT is nonzero when processing an indirect call, for
     example a call through a function pointer.  The value of INDIRECT
     is zero for a call to an explicitly named function, a library
     function call, or when `INIT_CUMULATIVE_ARGS' is used to find
     arguments for the function being compiled.

     When processing a call to a compiler support library function,
     LIBNAME identifies which one.  It is a `symbol_ref' rtx which
     contains the name of the function, as a string.  LIBNAME is 0 when
     an ordinary C function call is being processed.  Thus, each time
     this macro is called, either LIBNAME or FNTYPE is nonzero, but
     never both of them at once.   */
#define INIT_CUMULATIVE_ARGS(CUM, FNTYPE, LIBNAME, INDIRECT, FOO)   (CUM)=0

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
#define FUNCTION_ARG(CUM, MODE, TYPE, NAMED)   r53_function_arg(&(CUM), (MODE), (TYPE), (NAMED))

/*   A C statement (sans semicolon) to update the summarizer variable
     CUM to advance past an argument in the argument list.  The values
     MODE, TYPE and NAMED describe that argument.  Once this is done,
     the variable CUM is suitable for analyzing the _following_
     argument with `FUNCTION_ARG', etc.

     This macro need not do anything if the argument in question was
     passed on the stack.  The compiler knows how to track the amount
     of stack space used for arguments without any special help.   */
#define FUNCTION_ARG_ADVANCE(CUM, MODE, TYPE, NAMED)   r53_function_arg_advance(&(CUM), (MODE), (TYPE), (NAMED))

/*   A C expression that is nonzero if REGNO is the number of a hard
     register in which function arguments are sometimes passed.  This
     does _not_ include implicit arguments such as the static chain and
     the structure-value address.  On many machines, no registers can be
     used for this purpose since all function arguments are pushed on
     the stack.   */
#define FUNCTION_ARG_REGNO_P(REGNO)   r53_function_arg_regno_p(REGNO)

#define RETURN_POPS_ARGS(FDECL, FUNTYPE, STKSIZE) 0

/*   A C expression that is nonzero if REGNO is the number of a hard
     register in which the values of called function may come back. */
#define FUNCTION_VALUE_REGNO_P(REGNO)  ((REGNO) == 1)

/* Define how to find the value returned by a library function
   assuming the value has mode MODE.  */
#define LIBCALL_VALUE(MODE)   r53_libcall_value(MODE)

/*   If the structure value address is not passed in a register, define
     `STRUCT_VALUE' as an expression returning an RTX for the place
     where the address is passed.  If it returns 0, the address is
     passed as an "invisible" first argument.   */
#define STRUCT_VALUE   0

#define DEFAULT_PCC_STRUCT_RETURN 0

/* Before the prologue, the return address is in the RETS register.  */
#define INCOMING_RETURN_ADDR_RTX   gen_rtx_REG(Pmode, RETURN_ADDR_REGNUM)

/*   A C expression whose value is RTL representing the value of the
     return address for the frame COUNT steps up from the current
     frame, after the prologue.  FRAMEADDR is the frame pointer of the
     COUNT frame, or the frame pointer of the COUNT - 1 frame if
     `RETURN_ADDR_IN_PREVIOUS_FRAME' is defined.

     The value of the expression must always be the correct address when
     COUNT is zero, but may be `NULL_RTX' if there is not way to
     determine the return address of other frames.   */
#define RETURN_ADDR_RTX(COUNT, FRAMEADDR)   r53_return_addr_rtx((COUNT), (FRAMEADDR))
#define DWARF_FRAME_RETURN_COLUMN   DWARF_FRAME_REGNUM(RETURN_ADDR_REGNUM)

/* Call instructions don't modify the stack pointer on R53.  */
#define INCOMING_FRAME_SP_OFFSET   0

/*   Offset from the frame pointer to the first local variable slot to
     be allocated.

     If `FRAME_GROWS_DOWNWARD', find the next slot's offset by
     subtracting the first slot's length from `STARTING_FRAME_OFFSET'.
     Otherwise, it is found by adding the length of the first slot to
     the value `STARTING_FRAME_OFFSET'.   */
#define STARTING_FRAME_OFFSET   0



#if 0
/* Describe how we implement __builtin_eh_return.  */
#define EH_RETURN_DATA_REGNO(N)	((N) < 2 ? (N) : INVALID_REGNUM)
#define EH_RETURN_STACKADJ_RTX	gen_rtx_REG (Pmode, REG_P2)
#define EH_RETURN_HANDLER_RTX \
    gen_frame_mem (Pmode, plus_constant (frame_pointer_rtx, UNITS_PER_WORD))
#endif


/* Addressing Modes */

/* Recognize any constant value that is a valid address.  */
#define CONSTANT_ADDRESS_P(X)   r53_constant_address_p(X)

/* Nonzero if the constant value X is a legitimate general operand.
   symbol_ref are not legitimate and will be put into constant pool.
   See force_const_mem().
   If -mno-pool, all constants are legitimate.
 */
#define LEGITIMATE_CONSTANT_P(X)   r53_legitimate_constant_p(GET_MODE(X), X)

/*   A number, the maximum number of registers that can appear in a
     valid memory address.  Note that it is up to you to specify a
     value equal to the maximum number that `GO_IF_LEGITIMATE_ADDRESS'
     would ever accept. */
#define MAX_REGS_PER_ADDRESS   2


/*
#define LEGITIMIZE_RELOAD_ADDRESS(x, mode, opnum, type, ind_levels, WIN)  \
  do { int win; (x) = r53_legitimize_reload_address(x, mode, opnum, (int)type, ind_levels, &win); if (win) goto WIN; } while (0)
*/


/* Size of things */

/* Value is 1 if truncating an integer of INPREC bits to OUTPREC bits
   is done just by pretending it is already truncated.  */
#define TRULY_NOOP_TRUNCATION(OUTPREC, INPREC)   ((INPREC) <= 32 || (OUTPREC) > 32)
/*#define TRULY_NOOP_TRUNCATION(OUTPREC, INPREC)   1*/

/*   A C expression that is nonzero if on this machine the number of
     bits actually used for the count of a shift operation is equal to
     the number of bits needed to represent the size of the object
     being shifted.  When this macro is non-zero, the compiler will
     assume that it is safe to omit a sign-extend, zero-extend, and
     certain bitwise `and' instructions that truncates the count of a
     shift operation.  On machines that have instructions that act on
     bit-fields at variable positions, which may include `bit test'
     instructions, a nonzero `SHIFT_COUNT_TRUNCATED' also enables
     deletion of truncations of the values that serve as arguments to
     bit-field instructions.

     If both types of instructions truncate the count (for shifts) and
     position (for bit-field operations), or if no variable-position
     bit-field instructions exist, you should define this macro.

     However, on some machines, such as the 80386 and the 680x0,
     truncation only applies to shift operations and not the (real or
     pretended) bit-field operations.  Define `SHIFT_COUNT_TRUNCATED'
     to be zero on such machines.  Instead, add patterns to the `md'
     file that include the implied truncation of the shift instructions.

     You need not define this macro if it would always have the value
     of zero.   */
#define SHIFT_COUNT_TRUNCATED   1

/* Max number of bytes we can move from memory to memory
   in one reasonably fast instruction.  */
#define MOVE_MAX   64

/* MOVE_MAX_PIECES is the number of bytes at a time which we can
   move efficiently, as opposed to  MOVE_MAX which is the maximum
   number of bytes we can move with a single instruction.  */
#define MOVE_MAX_PIECES   8

/* If a memory-to-memory move would take MOVE_RATIO or more simple
   move-instruction pairs, we will do a movmem or libcall instead.  */

#define MOVE_RATIO(speed)   5

/* If a clear memory operation would take CLEAR_RATIO or more simple
   move-instruction sequences, we will do a clrmem or libcall instead.  */

#define CLEAR_RATIO(speed)   5

/* STORAGE LAYOUT: target machine storage layout
   Define this macro as a C expression which is nonzero if accessing
   less than a word of memory (i.e. a `char' or a `short') is no
   faster than accessing a word of memory, i.e., if such access
   require more than one instruction or if there is no difference in
   cost between byte and (aligned) word loads.

   When this macro is not defined, the compiler will access a field by
   finding the smallest containing object; when it is defined, a
   fullword load will be used if alignment permits.  Unless bytes
   accesses are faster than word accesses, using word accesses is
   preferable since it may eliminate subsequent memory access if
   subsequent accesses occur to other fields in the same word of the
   structure, but to different bytes.  */
#define SLOW_BYTE_ACCESS    0
#define SLOW_SHORT_ACCESS   0

/* Define this if most significant bit is lowest numbered
   in instructions that operate on numbered bit-fields. */
#define BITS_BIG_ENDIAN     0

/* Define this if most significant byte of a word is the lowest numbered.
   We can't access bytes but if we could we would in the Big Endian order. */
#define BYTES_BIG_ENDIAN    0

/* Define this if most significant word of a multiword number is numbered. */
#define WORDS_BIG_ENDIAN    0

/* number of bits in an addressable storage unit */
#define BITS_PER_UNIT       8

/* Width in bits of a "word", which is the contents of a machine register.
   Note that this is not necessarily the width of data type `int';
   if using 16-bit ints on a 68000, this would still be 32.
   But on a machine with 16-bit registers, this would be 16.  */
#define BITS_PER_WORD      64


/* Width of a word, in units (bytes).  */
#define UNITS_PER_WORD      8

#define UNITS_PER_SIMD_WORD(MODE)   64

/*   Minimum number of units in a word.  If this is undefined, the
     default is `UNITS_PER_WORD'.  Otherwise, it is the constant value
     that is the smallest value that `UNITS_PER_WORD' can have at
     run-time.   */
#define MIN_UNITS_PER_WORD  4  /* Keeps libgcc2.h from bombing */

/* Width in bits of a pointer.
   See also the macro `Pmode1' defined below.  */
#define POINTER_SIZE   GET_MODE_BITSIZE(Pmode)

/* Allocation boundary (in *bits*) for storing arguments in argument list.  */
#define PARM_BOUNDARY      64

/* Boundary (in *bits*) on which stack pointer should be aligned.  */
#define STACK_BOUNDARY    512

/* Allocation boundary (in *bits*) for the code of a function.  */
#define FUNCTION_BOUNDARY   32
#define FUNCTION_MODE       SImode

/* Specify the machine mode that this machine uses
   for the index in the tablejump instruction.  */
#define CASE_VECTOR_MODE   SImode

/* No data type wants to be aligned rounder than this.  */
#define BIGGEST_ALIGNMENT 512

/*   TARGET OS KEEPS A VECTOR-ALIGNED (512-bit, 64-byte) stack.  */
#define TARGET_KEEPS_VECTOR_ALIGNED_STACK  1

/* Define this if move instructions will actually fail to work
   when given unaligned data.  */
#define STRICT_ALIGNMENT   1

/*   Alignment in bits to be given to a structure bit-field that
     follows an empty field such as `int : 0;'.
     Note that `PCC_BITFIELD_TYPE_MATTERS' also affects the alignment
     that results from an empty field.   */
#define EMPTY_FIELD_BOUNDARY   32  /* Be compatible with i386 */

/*   Define this if you wish to imitate the way many other C compilers
     handle alignment of bit-fields and the structures that contain
     them.   */
#define PCC_BITFIELD_TYPE_MATTERS   1

#define CHAR_TYPE_SIZE	       8
#define SHORT_TYPE_SIZE       16
#define INT_TYPE_SIZE         32
#define LONG_TYPE_SIZE        (TARGET_64BIT ? 64 : 32)
#define LONG_LONG_TYPE_SIZE   64
#define FLOAT_TYPE_SIZE       32
#define DOUBLE_TYPE_SIZE      64
#define LONG_DOUBLE_TYPE_SIZE 64

/*   Width of a pointer, in bits.  You must specify a value no wider
     than the width of `Pmode'.  If it is not equal to the width of
     `Pmode', you must define `POINTERS_EXTEND_UNSIGNED'.   */
#define POINTER_SIZE   GET_MODE_BITSIZE(Pmode)

/* Define this as 1 if `char' should by default be signed; else as 0.  */
#define DEFAULT_SIGNED_CHAR 1

/* `PROMOTE_MODE (M, UNSIGNEDP, TYPE)'
     A macro to update M and UNSIGNEDP when an object whose type is
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

     Do not define this macro if it would never modify M.*/
#define PROMOTE_MODE(M, UNSIGNEDP, TYPE)   r53_promote_mode(&(M), &(UNSIGNEDP))

/*   Define this macro to be a C expression indicating when insns that
     read memory in MODE, an integral mode narrower than a word, set the
     bits outside of MODE to be either the sign-extension or the
     zero-extension of the data read.  Return `SIGN_EXTEND' for values
     of MODE for which the insn sign-extends, `ZERO_EXTEND' for which
     it zero-extends, and `NIL' for other modes.

     This macro is not called with MODE non-integral or with a width
     greater than or equal to `BITS_PER_WORD', so you may return any
     value in this case.  Do not define this macro if it would always
     return `NIL'.  On machines where this macro is defined, you will
     normally define it as the constant `SIGN_EXTEND' or `ZERO_EXTEND'.   */
#define LOAD_EXTEND_OP(MODE)   SIGN_EXTEND

#define SHORT_IMMEDIATES_SIGN_EXTEND   1

/* Define if operations between registers always perform the operation
   on the full register even if a narrower mode is specified. */
#define WORD_REGISTER_OPERATIONS   1

/*   A C expression for the size in bytes of the trampoline, as an
     integer.   */
#define TRAMPOLINE_SIZE       32
#define TRAMPOLINE_ALIGNMENT  64



/* Describing Relative Costs of Operations */

/* Do not put function addr into constant pool */
#define NO_FUNCTION_CSE   1


/*#define BRANCH_COST(SPEED, PREDICTABLE)  6*/
#define BRANCH_COST(SPEED, PREDICTABLE)  6
#define MAX_CONDITIONAL_EXECUTE   12

/*
#define IFCVT_MODIFY_TESTS(ce_info, true_expr, false_expr)   r53_ifcvt_modify_tests(ce_info, true_expr, false_expr)   
*/

/* A C expression for the cost of moving data from a register in class FROM to
   one in class TO.  The classes are expressed using the enumeration values
   such as `GENERAL_REGS'.  A value of 2 is the default; other values are
   interpreted relative to that.

   It is not required that the cost always equal 2 when FROM is the same as TO;
   on some machines it is expensive to move between registers if they are not
   general registers.  */

#if 0
#define REGISTER_MOVE_COST(MODE, CLASS1, CLASS2) \
   bfin_register_move_cost ((MODE), (CLASS1), (CLASS2))

/* A C expression for the cost of moving data of mode M between a
   register and memory.  A value of 2 is the default; this cost is
   relative to those in `REGISTER_MOVE_COST'.

   If moving between registers and memory is more expensive than
   between two registers, you should define this macro to express the
   relative cost.  */

#define MEMORY_MOVE_COST(MODE, CLASS, IN)	\
  bfin_memory_move_cost ((MODE), (CLASS), (IN))
#endif


#define PRINT_OPERAND(FILE, RTX, CODE)	 r53_print_operand(FILE, RTX, CODE)

/*   A C compound statement to output to stdio stream STREAM the
     assembler syntax for an instruction operand that is a memory
     reference whose address is X.  X is an RTL expression.  */
#define PRINT_OPERAND_ADDRESS(FILE, RTX) r53_print_operand_address(FILE, RTX)

#define TEXT_SECTION_ASM_OP	".text;"
#define DATA_SECTION_ASM_OP	".data;"

#define GLOBAL_ASM_OP		"\t.global "

/*   This macro should be provided on machines where the addresses in a
     dispatch table are absolute. */
#define ASM_OUTPUT_ADDR_VEC_ELT(STREAM, VALUE)   fprintf((STREAM), "\t.long .L%d\n", (VALUE))

/*   A C statement to output to the stdio stream STREAM an assembler
     command to advance the location counter to a multiple of 2 to the
     POWER bytes.  POWER will be a C expression of type `int'.   */
#define ASM_OUTPUT_ALIGN(STREAM, POWER)   r53_asm_output_align((STREAM), (POWER))

/*   A C statement or compound statement to output to FILE some
     assembler code to call the profiling subroutine `mcount'.

     The details of how `mcount' expects to be called are determined by
     your operating system environment, not by GCC.  To figure them out,
     compile a small program for profiling using the system's installed
     C compiler and look at the assembler code that results.

     Older implementations of `mcount' expect the address of a counter
     variable to be loaded into some register.  The name of this
     variable is `LP' followed by the number LABELNO, so you would
     generate the name using `LP%d' in a `fprintf'.   */
#define FUNCTION_PROFILER(FILE, LABELNO)


/*   A C expression that defines the machine-dependent operand
     constraint letters (`I', `J', `K', ... `P') that specify
     particular ranges of integer values.  If C is one of those
     letters, the expression should check that VALUE, an integer, is in
     the appropriate range and return 1 if so, 0 otherwise.  If C is
     not one of those letters, the value should be 0 regardless of
     VALUE.   */
#define CONST_OK_FOR_LETTER_P(VALUE, C)   r53_const_ok_for_letter_p((VALUE), (C))

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
#define CONST_DOUBLE_OK_FOR_LETTER_P(VALUE, C)   r53_const_double_ok_for_letter_p((VALUE), (C))

/*   A C expression that defines the optional machine-dependent
     constraint letters that can be used to segregate specific types of
     operands, usually memory references, for the target machine.  Any
     letter that is not elsewhere defined and not matched by
     `REG_CLASS_FROM_LETTER' may be used.  Normally this macro will not
     be defined.

     If it is required for a particular target machine, it should
     return 1 if VALUE corresponds to the operand type represented by
     the constraint letter C.  If C is not defined as an extra
     constraint, the value returned should be 0 regardless of VALUE.

     For example, on the ROMP, load instructions cannot have their
     output in r0 if the memory reference contains a symbolic address.
     Constraint letter `Q' is defined as representing a memory address
     that does _not_ contain a symbolic address.  An alternative is
     specified with a `Q' constraint on the input and `r' on the
     output.  The next alternative specifies `m' on the input and a
     register class that does not include r0 on the output.   */
#ifdef REG_OK_STRICT
#define EXTRA_CONSTRAINT(VALUE, C)   r53_extra_constraint((VALUE), (C), 1)
#else
#define EXTRA_CONSTRAINT(VALUE, C)   r53_extra_constraint((VALUE), (C), 0)
#endif

#define EXTRA_MEMORY_CONSTRAINT(c, str)  (c == 'S' || c == 'T')


#undef ASM_OUTPUT_CASE_LABEL   /* Stop elfos.h from emiting .align directive */


#endif /*  _R53_CONFIG */
