#ifndef __UTIL_H
#define __UTIL_H

#include <errno.h>
#include <fcntl.h>

#include <sys/ioctl.h>
#include <sys/mman.h>

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>

#define GENMASK_ULL(up, lo) (unsigned long)(((1UL << ((up) - (lo))) - 1UL) << (lo))

/* The char device that allows mapping arbitrary memory */
#define DEVMEM_STR		"/dev/mem_poke"

#define DEVMEM_ALLOC_PAGE	_IOC(_IOC_WRITE, 'k', 4, 0)

/* Assuming a 4Kb page size */
#define PAGE_SHIFT		(12UL)
#define PAGE_SIZE		(1UL << PAGE_SHIFT)

int read_memory(unsigned long pa, void *to, size_t len);
int write_memory(unsigned long pa, const void *from, size_t len);
unsigned long allocate_kernel_page(void);

void cleanup(void);

#endif /* __UTIL_H */
