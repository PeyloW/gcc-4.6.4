/* libgcc routines for 68000 w/o floating-point hardware.
   Copyright (C) 1994, 1996, 1997, 1998, 2008, 2009 Free Software Foundation, Inc.

This file is part of GCC.

GCC is free software; you can redistribute it and/or modify it
under the terms of the GNU General Public License as published by the
Free Software Foundation; either version 3, or (at your option) any
later version.

This file is distributed in the hope that it will be useful, but
WITHOUT ANY WARRANTY; without even the implied warranty of
MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the GNU
General Public License for more details.

Under Section 7 of GPL version 3, you are granted additional
permissions described in the GCC Runtime Library Exception, version
3.1, as published by the Free Software Foundation.

You should have received a copy of the GNU General Public License and
a copy of the GCC Runtime Library Exception along with this program;
see the files COPYING3 and COPYING.RUNTIME respectively.  If not, see
<http://www.gnu.org/licenses/>.  */

/* Use this one for any 680x0; assumes no floating point hardware.
   The trailing " '" appearing on some lines is for ANSI preprocessors.  Yuk.
   Some of this code comes from MINIX, via the folks at ericsson.
   D. V. Henkel-Wallace (gumby@cygnus.com) Fete Bastille, 1992
*/

/* These are predefined by new versions of GNU cpp.  */

#ifndef __USER_LABEL_PREFIX__
#define __USER_LABEL_PREFIX__ _
#endif

#ifndef __REGISTER_PREFIX__
#define __REGISTER_PREFIX__
#endif

#ifndef __IMMEDIATE_PREFIX__
#define __IMMEDIATE_PREFIX__ #
#endif

/* ANSI concatenation macros.  */

#define CONCAT1(a, b) CONCAT2(a, b)
#define CONCAT2(a, b) a ## b

/* Use the right prefix for global labels.  */

#define SYM(x) CONCAT1 (__USER_LABEL_PREFIX__, x)

/* Note that X is a function.  */
	
#ifdef __ELF__
#define FUNC(x) .type SYM(x),function
#else
/* The .proc pseudo-op is accepted, but ignored, by GAS.  We could just	
   define this to the empty string for non-ELF systems, but defining it
   to .proc means that the information is available to the assembler if
   the need arises.  */
#define FUNC(x) .proc
#endif
		
/* Use the right prefix for registers.  */

#define REG(x) CONCAT1 (__REGISTER_PREFIX__, x)

/* Use the right prefix for immediate values.  */

#define IMM(x) CONCAT1 (__IMMEDIATE_PREFIX__, x)

#define d0 REG (d0)
#define d1 REG (d1)
#define d2 REG (d2)
#define d3 REG (d3)
#define d4 REG (d4)
#define d5 REG (d5)
#define d6 REG (d6)
#define d7 REG (d7)
#define a0 REG (a0)
#define a1 REG (a1)
#define a2 REG (a2)
#define a3 REG (a3)
#define a4 REG (a4)
#define a5 REG (a5)
#define a6 REG (a6)
#define fp REG (fp)
#define sp REG (sp)
#define pc REG (pc)

/* Provide a few macros to allow for PIC code support.
 * With PIC, data is stored A5 relative so we've got to take a bit of special
 * care to ensure that all loads of global data is via A5.  PIC also requires
 * jumps and subroutine calls to be PC relative rather than absolute.  We cheat
 * a little on this and in the PIC case, we use short offset branches and
 * hope that the final object code is within range (which it should be).
 */
#ifndef __PIC__

	/* Non PIC (absolute/relocatable) versions */

	.macro PICCALL addr
	jbsr	\addr
	.endm

	.macro PICJUMP addr
	jmp	\addr
	.endm

	.macro PICLEA sym, reg
	lea	\sym, \reg
	.endm

	.macro PICPEA sym, areg
	pea	\sym
	.endm

#else /* __PIC__ */

# if defined (__uClinux__)

	/* Versions for uClinux */

#  if defined(__ID_SHARED_LIBRARY__)

	/* -mid-shared-library versions  */

	.macro PICLEA sym, reg
	movel	a5@(_current_shared_library_a5_offset_), \reg
	movel	\sym@GOT(\reg), \reg
	.endm

	.macro PICPEA sym, areg
	movel	a5@(_current_shared_library_a5_offset_), \areg
	movel	\sym@GOT(\areg), sp@-
	.endm

	.macro PICCALL addr
	PICLEA	\addr,a0
	jsr	a0@
	.endm

	.macro PICJUMP addr
	PICLEA	\addr,a0
	jmp	a0@
	.endm

#  else /* !__ID_SHARED_LIBRARY__ */

	/* Versions for -msep-data */

	.macro PICLEA sym, reg
	movel	\sym@GOT(a5), \reg
	.endm

	.macro PICPEA sym, areg
	movel	\sym@GOT(a5), sp@-
	.endm

	.macro PICCALL addr
#if defined (__mcoldfire__) && !defined (__mcfisab__) && !defined (__mcfisac__)
	lea	\addr-.-8,a0
	jsr	pc@(a0)
#else
	jbsr	\addr
#endif
	.endm

	.macro PICJUMP addr
	/* ISA C has no bra.l instruction, and since this assembly file
	   gets assembled into multiple object files, we avoid the
	   bra instruction entirely.  */
#if defined (__mcoldfire__) && !defined (__mcfisab__)
	lea	\addr-.-8,a0
	jmp	pc@(a0)
#else
	bra	\addr
#endif
	.endm

#  endif

# else /* !__uClinux__ */

	/* Versions for Linux */

	.macro PICLEA sym, reg
	movel	#_GLOBAL_OFFSET_TABLE_@GOTPC, \reg
	lea	(-6, pc, \reg), \reg
	movel	\sym@GOT(\reg), \reg
	.endm

	.macro PICPEA sym, areg
	movel	#_GLOBAL_OFFSET_TABLE_@GOTPC, \areg
	lea	(-6, pc, \areg), \areg
	movel	\sym@GOT(\areg), sp@-
	.endm

	.macro PICCALL addr
#if defined (__mcoldfire__) && !defined (__mcfisab__) && !defined (__mcfisac__)
	lea	\addr-.-8,a0
	jsr	pc@(a0)
#else
	jbsr	\addr
#endif
	.endm

	.macro PICJUMP addr
	/* ISA C has no bra.l instruction, and since this assembly file
	   gets assembled into multiple object files, we avoid the
	   bra instruction entirely.  */
#if defined (__mcoldfire__) && !defined (__mcfisab__)
	lea	\addr-.-8,a0
	jmp	pc@(a0)
#else
	bra	\addr
#endif
	.endm

# endif
#endif /* __PIC__ */

#ifndef __FASTCALL__
#include "lb1sf68-std.asm"
#else
#include "lb1sf68-fast.asm"
#endif


#if defined (__ELF__) && defined (__linux__)
	/* Make stack non-executable for ELF linux targets.  */
	.section	.note.GNU-stack,"",@progbits
#endif
