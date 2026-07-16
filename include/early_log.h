#ifndef _EARLY_LOG_H
#define _EARLY_LOG_H

#include <linux/types.h>
#include <asm/io.h>
#include <asm/system.h>

#define EARLY_LOG_BASE	0x43FFF000
#define EARLY_LOG_SIZE	0x1000
#define EARLY_LOG_MAGIC	0x454C4F47

static inline void early_log_write(const char *msg)
{
	volatile u32 *magic = (volatile u32 *)EARLY_LOG_BASE;
	volatile u32 *idx = (volatile u32 *)(EARLY_LOG_BASE + 4);
	volatile char *buf = (volatile char *)(EARLY_LOG_BASE + 8);
	u32 pos;
	int i;

	if (*magic != EARLY_LOG_MAGIC) {
		*magic = EARLY_LOG_MAGIC;
		*idx = 0;
	}

	pos = *idx;
	for (i = 0; msg[i]; i++) {
		if (pos + 1 >= EARLY_LOG_SIZE - 8) {
			pos = 0;
		}
		buf[pos++] = msg[i];
		buf[pos] = '\0';
	}
	*idx = pos;

	if (get_sctlr() & CR_C) {
		unsigned long adr;
		asm volatile("dsb sy");
		for (adr = EARLY_LOG_BASE;
		     adr < EARLY_LOG_BASE + EARLY_LOG_SIZE;
		     adr += 64)
			asm volatile("dc cvac, %0" :: "r"(adr));
		asm volatile("dsb sy");
	}
}

#define EARLY_W(msg) do { early_log_write("[EARLY] " msg "\n"); } while (0)

#endif
