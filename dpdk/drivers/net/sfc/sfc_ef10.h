/* SPDX-License-Identifier: BSD-3-Clause
 *
 * Copyright (c) 2017-2018 Solarflare Communications Inc.
 * All rights reserved.
 *
 * This software was jointly developed between OKTET Labs (under contract
 * for Solarflare) and Solarflare Communications, Inc.
 */

#ifndef _SFC_EF10_H
#define _SFC_EF10_H

#ifdef __cplusplus
extern "C" {
#endif

/* Number of events in one cache line */
#define SFC_EF10_EV_PER_CACHE_LINE \
	(RTE_CACHE_LINE_SIZE / sizeof(efx_qword_t))

#define SFC_EF10_EV_QCLEAR_MASK		(~(SFC_EF10_EV_PER_CACHE_LINE - 1))

#if defined(SFC_EF10_EV_QCLEAR_USE_EFX)
static inline void
sfc_ef10_ev_qclear_cache_line(void *ptr)
{
	efx_qword_t *entry = ptr;
	unsigned int i;

	for (i = 0; i < SFC_EF10_EV_PER_CACHE_LINE; ++i)
		EFX_SET_QWORD(entry[i]);
}
#else
/*
 * It is possible to do it using AVX2 and AVX512F, but it shows less
 * performance.
 */
static inline void
sfc_ef10_ev_qclear_cache_line(void *ptr)
{
	const __m128i val = _mm_set1_epi64x(UINT64_MAX);
	__m128i *addr = ptr;
	unsigned int i;

	RTE_BUILD_BUG_ON(sizeof(val) > RTE_CACHE_LINE_SIZE);
	RTE_BUILD_BUG_ON(RTE_CACHE_LINE_SIZE % sizeof(val) != 0);

	for (i = 0; i < RTE_CACHE_LINE_SIZE / sizeof(val); ++i)
		_mm_store_si128(&addr[i], val);
}
#endif

static inline void
sfc_ef10_ev_qclear(efx_qword_t *hw_ring, unsigned int ptr_mask,
		   unsigned int old_read_ptr, unsigned int read_ptr)
{
	const unsigned int clear_ptr = read_ptr & SFC_EF10_EV_QCLEAR_MASK;
	unsigned int old_clear_ptr = old_read_ptr & SFC_EF10_EV_QCLEAR_MASK;

	while (old_clear_ptr != clear_ptr) {
		sfc_ef10_ev_qclear_cache_line(
			&hw_ring[old_clear_ptr & ptr_mask]);
		old_clear_ptr += SFC_EF10_EV_PER_CACHE_LINE;
	}

	/*
	 * No barriers here.
	 * Functions which push doorbell should care about correct
	 * ordering: store instructions which fill in EvQ ring should be
	 * retired from CPU and DMA sync before doorbell which will allow
	 * to use these event entries.
	 */
}

static inline bool
sfc_ef10_ev_present(const efx_qword_t ev)
{
	return ~EFX_QWORD_FIELD(ev, EFX_DWORD_0) |
	       ~EFX_QWORD_FIELD(ev, EFX_DWORD_1);
}

#ifdef __cplusplus
}
#endif
#endif /* _SFC_EF10_H */
