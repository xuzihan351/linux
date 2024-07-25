/* SPDX-License-Identifier: GPL-2.0-or-later */
/*
 * Copyright (C) 2007 Red Hat, Inc. All Rights Reserved.
 * Copyright (C) 2012 Regents of the University of California
 * Copyright (C) 2017 SiFive
 */

#ifndef _ASM_RISCV_ATOMIC_H
#define _ASM_RISCV_ATOMIC_H

#ifdef CONFIG_GENERIC_ATOMIC64
# include <asm-generic/atomic64.h>
#else
# if (__riscv_xlen < 64)
#  error "64-bit atomics require XLEN to be at least 64"
# endif
#endif

#include <linux/irqflags.h>
#include <asm/cmpxchg.h>

#define __atomic_acquire_fence()					\
	__asm__ __volatile__(RISCV_ACQUIRE_BARRIER "" ::: "memory")

#define __atomic_release_fence()					\
	__asm__ __volatile__(RISCV_RELEASE_BARRIER "" ::: "memory");

static __always_inline int arch_atomic_read(const atomic_t *v)
{
	return READ_ONCE(v->counter);
}
static __always_inline void arch_atomic_set(atomic_t *v, int i)
{
	WRITE_ONCE(v->counter, i);
}

#ifndef CONFIG_GENERIC_ATOMIC64
#define ATOMIC64_INIT(i) { (i) }
static __always_inline s64 arch_atomic64_read(const atomic64_t *v)
{
	return READ_ONCE(v->counter);
}
static __always_inline void arch_atomic64_set(atomic64_t *v, s64 i)
{
	WRITE_ONCE(v->counter, i);
}
#endif

#define ATOMIC_OP(op_name, op)		\
static __always_inline void arch_atomic_##op_name(int i, atomic_t *v) 	\
{									\
	unsigned long flags;						\
	raw_local_irq_save(flags);					\
	v->counter op i;						\
	raw_local_irq_restore(flags);					\
}

ATOMIC_OP(add, +=)
ATOMIC_OP(sub, -=)
ATOMIC_OP(and, &=)
ATOMIC_OP( or, |=)
ATOMIC_OP(xor, ^=)

#undef ATOMIC_OP

static __always_inline int arch_atomic_fetch_add(int i, atomic_t *v)
{
	unsigned long flags;
	int ret;

	raw_local_irq_save(flags);
	ret = v->counter;
	v->counter += i;
	raw_local_irq_restore(flags);

	return ret;
}

static __always_inline int arch_atomic_add_return(int i, atomic_t *v)
{
	return arch_atomic_fetch_add(i, v) + i;
}

static __always_inline int arch_atomic_fetch_sub(int i, atomic_t *v)
{
	return arch_atomic_fetch_add(-i, v);
}

static __always_inline int arch_atomic_sub_return(int i, atomic_t *v)
{
	return arch_atomic_add_return(-i, v);
}

#define arch_atomic_add_return_relaxed	arch_atomic_add_return
#define arch_atomic_sub_return_relaxed	arch_atomic_sub_return
#define arch_atomic_add_return		arch_atomic_add_return
#define arch_atomic_sub_return		arch_atomic_sub_return

#define arch_atomic_fetch_add_relaxed	arch_atomic_fetch_add
#define arch_atomic_fetch_sub_relaxed	arch_atomic_fetch_sub
#define arch_atomic_fetch_add		arch_atomic_fetch_add
#define arch_atomic_fetch_sub		arch_atomic_fetch_sub

#ifndef CONFIG_GENERIC_ATOMIC64
#define arch_atomic64_add_return_relaxed	arch_atomic64_add_return_relaxed
#define arch_atomic64_sub_return_relaxed	arch_atomic64_sub_return_relaxed
#define arch_atomic64_add_return		arch_atomic64_add_return
#define arch_atomic64_sub_return		arch_atomic64_sub_return

#define arch_atomic64_fetch_add_relaxed	arch_atomic64_fetch_add_relaxed
#define arch_atomic64_fetch_sub_relaxed	arch_atomic64_fetch_sub_relaxed
#define arch_atomic64_fetch_add		arch_atomic64_fetch_add
#define arch_atomic64_fetch_sub		arch_atomic64_fetch_sub
#endif

#define ATOMIC_FETCH_OP(op_name, op)						\
static __always_inline int arch_atomic_fetch_##op_name(int i, atomic_t *v)	\
{						 				\
	unsigned long flags;							\
	int ret;								\
	raw_local_irq_save(flags);						\
	ret = v->counter;							\
	v->counter op i;							\
	raw_local_irq_restore(flags);						\
	return ret;								\
}
ATOMIC_FETCH_OP(and, &=)
ATOMIC_FETCH_OP( or, |=)
ATOMIC_FETCH_OP(xor, ^=)

#define arch_atomic_fetch_and_relaxed	arch_atomic_fetch_and
#define arch_atomic_fetch_or_relaxed	arch_atomic_fetch_or
#define arch_atomic_fetch_xor_relaxed	arch_atomic_fetch_xor
#define arch_atomic_fetch_and		arch_atomic_fetch_and
#define arch_atomic_fetch_or		arch_atomic_fetch_or
#define arch_atomic_fetch_xor		arch_atomic_fetch_xor

#ifndef CONFIG_GENERIC_ATOMIC64
#define arch_atomic64_fetch_and_relaxed	arch_atomic64_fetch_and_relaxed
#define arch_atomic64_fetch_or_relaxed	arch_atomic64_fetch_or_relaxed
#define arch_atomic64_fetch_xor_relaxed	arch_atomic64_fetch_xor_relaxed
#define arch_atomic64_fetch_and		arch_atomic64_fetch_and
#define arch_atomic64_fetch_or		arch_atomic64_fetch_or
#define arch_atomic64_fetch_xor		arch_atomic64_fetch_xor
#endif

#undef ATOMIC_FETCH_OP

#define _arch_atomic_fetch_add_unless(_prev, _rc, counter, _a, _u, sfx)	\
({									\
	unsigned long __flags;						\
									\
	raw_local_save_flags(__flags);					\
	__asm__ __volatile__ (						\
		"	lw             %[p],  %[c]\n"			\
		"	beq	       %[p],  %[u], 1f\n"		\
		"	add            %[rc], %[p], %[a]\n"		\
		"	sw             %[rc], %[c]\n"			\
		"	mv             %[rc], zero\n"			\
		"	fence          rw, rw\n"			\
		"1:\n"							\
		: [p]"=&r" (_prev), [rc]"=&r" (_rc), [c]"+A" (counter)	\
		: [a]"r" (_a), [u]"r" (_u)				\
		: "memory");						\
	raw_local_irq_restore(__flags);					\
})

/* This is required to provide a full barrier on success. */
static __always_inline int arch_atomic_fetch_add_unless(atomic_t *v, int a, int u)
{
       int prev, rc;

	_arch_atomic_fetch_add_unless(prev, rc, v->counter, a, u, "w");

	return prev;
}
#define arch_atomic_fetch_add_unless arch_atomic_fetch_add_unless

#ifndef CONFIG_GENERIC_ATOMIC64
static __always_inline s64 arch_atomic64_fetch_add_unless(atomic64_t *v, s64 a, s64 u)
{
       s64 prev;
       long rc;

	_arch_atomic_fetch_add_unless(prev, rc, v->counter, a, u, "d");

	return prev;
}
#define arch_atomic64_fetch_add_unless arch_atomic64_fetch_add_unless
#endif

#define _arch_atomic_inc_unless_negative(_prev, _rc, counter, sfx)	\
({									\
	unsigned long __flags;						\
									\
	raw_local_save_flags(__flags);					\
	__asm__ __volatile__ (						\
		"	lw              %[p],  %[c]\n"			\
		"	bltz            %[p],  1f\n"			\
		"	addi            %[rc], %[p], 1\n"		\
		"	sw              %[rc], %[c]\n"			\
		"	mv              %[rc], zero\n"			\
		"	fence           rw, rw\n"			\
		"1:\n"							\
		: [p]"=&r" (_prev), [rc]"=&r" (_rc), [c]"+A" (counter)	\
		:							\
		: "memory");						\
	raw_local_irq_restore(__flags);					\
})

static __always_inline bool arch_atomic_inc_unless_negative(atomic_t *v)
{
	int prev, rc;

	_arch_atomic_inc_unless_negative(prev, rc, v->counter, "w");

	return !(prev < 0);
}

#define arch_atomic_inc_unless_negative arch_atomic_inc_unless_negative

#define _arch_atomic_dec_unless_positive(_prev, _rc, counter, sfx)	\
({									\
	unsigned long __flags;						\
									\
	raw_local_save_flags(__flags);					\
	__asm__ __volatile__ (						\
		"	lw              %[p],  %[c]\n"			\
		"	bgtz            %[p],  1f\n"			\
		"	addi            %[rc], %[p], -1\n"		\
		"	sw              %[rc], %[c]\n"			\
		"	mv              %[rc], zero\n"			\
		"	fence           rw, rw\n"			\
		"1:\n"							\
		: [p]"=&r" (_prev), [rc]"=&r" (_rc), [c]"+A" (counter)	\
		:							\
		: "memory");						\
	raw_local_irq_restore(__flags);					\
})

static __always_inline bool arch_atomic_dec_unless_positive(atomic_t *v)
{
	int prev, rc;

	_arch_atomic_dec_unless_positive(prev, rc, v->counter, "w");

	return !(prev > 0);
}

#define arch_atomic_dec_unless_positive arch_atomic_dec_unless_positive

#define _arch_atomic_dec_if_positive(_prev, _rc, counter, sfx)		\
({									\
	unsigned long __flags;						\
									\
	raw_local_save_flags(__flags);					\
	__asm__ __volatile__ (						\
		"	lw             %[p],  %[c]\n"			\
		"	addi           %[rc], %[p], -1\n"		\
		"	bltz           %[rc], 1f\n"			\
		"	sw             %[rc], %[c]\n"			\
		"	mv             %[rc], zero\n"			\
		"	fence          rw, rw\n"			\
		"1:\n"							\
		: [p]"=&r" (_prev), [rc]"=&r" (_rc), [c]"+A" (counter)	\
		:							\
		: "memory");						\
	raw_local_irq_restore(__flags);					\
})

static __always_inline int arch_atomic_dec_if_positive(atomic_t *v)
{
       int prev, rc;

	_arch_atomic_dec_if_positive(prev, rc, v->counter, "w");

	return prev - 1;
}

#define arch_atomic_dec_if_positive arch_atomic_dec_if_positive

#ifndef CONFIG_GENERIC_ATOMIC64
static __always_inline bool arch_atomic64_inc_unless_negative(atomic64_t *v)
{
	s64 prev;
	long rc;

	_arch_atomic_inc_unless_negative(prev, rc, v->counter, "d");

	return !(prev < 0);
}

#define arch_atomic64_inc_unless_negative arch_atomic64_inc_unless_negative

static __always_inline bool arch_atomic64_dec_unless_positive(atomic64_t *v)
{
	s64 prev;
	long rc;

	_arch_atomic_dec_unless_positive(prev, rc, v->counter, "d");

	return !(prev > 0);
}

#define arch_atomic64_dec_unless_positive arch_atomic64_dec_unless_positive

static __always_inline s64 arch_atomic64_dec_if_positive(atomic64_t *v)
{
       s64 prev;
       long rc;

	_arch_atomic_dec_if_positive(prev, rc, v->counter, "d");

	return prev - 1;
}

#define arch_atomic64_dec_if_positive	arch_atomic64_dec_if_positive
#endif

#endif /* _ASM_RISCV_ATOMIC_H */
