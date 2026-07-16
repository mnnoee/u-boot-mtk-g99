#ifndef _EARLY_LOG_H
#define _EARLY_LOG_H

#include <linux/types.h>
#include <asm/io.h>
#include <asm/system.h>

/*
 * 16MB early log buffer at 0x48000000
 * Header (16 bytes): magic:4 + idx:4 + seq:4 + pad:4
 * Data: 16MB - 16 bytes
 * Cache flush: only the 64-byte lines touched per write
 */
#define EARLY_LOG_BASE	0x48000000
#define EARLY_LOG_HDRSZ	16
#define EARLY_LOG_SIZE	0x1000000	/* 16 MB */
#define EARLY_LOG_BUFSZ	(EARLY_LOG_SIZE - EARLY_LOG_HDRSZ)
#define EARLY_LOG_MAGIC	0x454C4F47

static inline void early_log_write(const char *msg)
{
	volatile u32 *magic = (volatile u32 *)EARLY_LOG_BASE;
	volatile u32 *idx   = (volatile u32 *)(EARLY_LOG_BASE + 4);
	volatile u32 *seq   = (volatile u32 *)(EARLY_LOG_BASE + 8);
	volatile char *buf  = (volatile char *)(EARLY_LOG_BASE + EARLY_LOG_HDRSZ);
	u32 pos, old_pos;
	int i;

	if (*magic != EARLY_LOG_MAGIC) {
		*magic = EARLY_LOG_MAGIC;
		*idx = 0;
		*seq = 0;
	}

	old_pos = *idx;
	pos = old_pos;

	for (i = 0; msg[i]; i++) {
		if (pos + 1 >= EARLY_LOG_BUFSZ)
			pos = 0;
		buf[pos++] = msg[i];
	}
	buf[pos] = '\0';
	*idx = pos;
	*seq = *seq + 1;

	/* Flush only the cache lines containing newly written bytes */
	if (get_sctlr() & CR_C) {
		unsigned long a, end_a;
		asm volatile("dsb sy");

		a = (unsigned long)&buf[old_pos];
		end_a = (unsigned long)&buf[pos];

		if (a <= end_a) {
			/* Normal: flush from old_pos to pos */
			for (a = a & ~63UL; a <= end_a; a += 64)
				asm volatile("dc cvac, %0" :: "r"(a));
		} else {
			/* Wrapped: flush end of buffer + start of buffer */
			for (a = a & ~63UL; a < (unsigned long)&buf[EARLY_LOG_BUFSZ]; a += 64)
				asm volatile("dc cvac, %0" :: "r"(a));
			for (a = (unsigned long)buf & ~63UL; a <= end_a; a += 64)
				asm volatile("dc cvac, %0" :: "r"(a));
		}
		asm volatile("dsb sy");
	}
}

#define EARLY_W(msg) do { early_log_write("[EARLY] " msg "\n"); } while (0)

#endif
