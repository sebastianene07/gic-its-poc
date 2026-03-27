#include "util.h"
#include "btree.h"

static int devmem_fd = -1;

struct TreeNode *root_node;

static void *get_mapped_va(unsigned long pa)
{
	void *mmap_base_page;
	struct TreeNode *node = search_node(root_node, pa);
	if (!node) {
		if (devmem_fd < 0) {
			devmem_fd = open(DEVMEM_STR, O_RDWR | O_SYNC);
			if (devmem_fd < 0) {
				fprintf(stderr, "The node %s is not available\n", DEVMEM_STR);
				return NULL;
			}
		}
		mmap_base_page = mmap(0, PAGE_SIZE, PROT_READ | PROT_WRITE, MAP_SHARED, devmem_fd,
				      pa & ~(PAGE_SIZE - 1));
		if (!mmap_base_page) {
			fprintf(stderr, "cannot mmap %d", errno);
			return NULL;
		}

		node = insert_node(root_node, pa, mmap_base_page);
		return mmap_base_page + (pa & (PAGE_SIZE - 1));
	}

	return node->data + (pa & (PAGE_SIZE - 1));
}

static void copy_helper(void *to, const void *from, size_t len)
{
	switch (len) {
		case 2:
			*(unsigned short *)to = *((const unsigned short *)from);
		case 4:
			*(unsigned int *)to = *((const unsigned int *)from);
			break;
		case 8:
			*(unsigned long *)to = *((const unsigned long *)from);
			break;
		default:
			memcpy(to, from, len);
			break;
	}
}

int read_memory(unsigned long pa, void *to, size_t len)
{
	void *mapped_va = get_mapped_va(pa);
	if (!mapped_va) {
		fprintf(stderr, "ERROR read pa 0x%lx not mapped\n", pa);
		return -EINVAL;
	}

	copy_helper(to, mapped_va, len);
	return 0;
}

int write_memory(unsigned long pa, const void *from, size_t len)
{
	void *mapped_va = get_mapped_va(pa);
	if (!mapped_va) {
		fprintf(stderr, "ERROR write pa 0x%lx not mapped\n", pa);
		return -EINVAL;
	}

	copy_helper(mapped_va, from, len);
	return 0;
}

unsigned long allocate_kernel_page(void)
{
	int ret;
	unsigned long kaddr;

	if (devmem_fd < 0) {
		devmem_fd = open(DEVMEM_STR, O_RDWR | O_SYNC);
		if (devmem_fd < 0) {
			fprintf(stderr, "The node %s is not available\n", DEVMEM_STR);
			return 0;
		}
	}

	ret = ioctl(devmem_fd, DEVMEM_ALLOC_PAGE, &kaddr);
	if (ret < 0) {
		fprintf(stderr, "ERROR %d ioctl DEVMEM_ALLOC_PAGE\n", ret);
		return 0;
	}

	return kaddr;
}

void cleanup(void)
{
	free_tree(root_node);
	close(devmem_fd);
}
