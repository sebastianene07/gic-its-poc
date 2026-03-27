#include "its.h"

static unsigned long its_get_baser_table(unsigned long *reg_mmio_pa, unsigned long table_type)
{
	unsigned long gits_baser, from = its_addr + GITS_BASER;
	unsigned long to = from + 8 * sizeof(gits_baser);

	while (from < to) {
		read_memory(from, &gits_baser, sizeof(gits_baser));
		if (GITS_BASER_TYPE(gits_baser) == table_type) {
			*reg_mmio_pa = from;
			return gits_baser;
		}

		from += sizeof(gits_baser);
	}

	return 0UL;
}

unsigned long its_get_device_table_baser(unsigned long *reg_mmio_pa)
{
	return its_get_baser_table(reg_mmio_pa, GITS_BASER_TYPE_DEVICE);
}

unsigned long its_get_collection_table_baser(unsigned long *reg_mmio_pa)
{
	return its_get_baser_table(reg_mmio_pa, GITS_BASER_TYPE_COLLECTION);
}

unsigned long its_get_vpe_table_baser(unsigned long *reg_mmio_pa)
{
	return its_get_baser_table(reg_mmio_pa, GITS_BASER_TYPE_VCPU);
}

static void get_next_command(unsigned long *next_cmd_pa, unsigned long *next_cwriter)
{
	unsigned long gits_cbaser, cmdq_pa, gits_cwriter;
	unsigned long cmdq_len;

	read_memory(its_addr + GITS_CBASER, &gits_cbaser, sizeof(gits_cbaser));
	read_memory(its_addr + GITS_CWRITER, &gits_cwriter, sizeof(gits_cwriter));

	cmdq_pa = GITS_CBASER_ADDRESS(gits_cbaser);
	cmdq_len = ((gits_cbaser & 0xFF) + 1) << PAGE_SHIFT;

	/* Check if we have space for a new command or we wrap around */
	if (gits_cwriter + 32 > cmdq_len) {
		gits_cwriter = 0;
		write_memory(its_addr + GITS_CWRITER, &gits_cwriter, sizeof(gits_cwriter));
	}

	*next_cmd_pa = cmdq_pa + gits_cwriter;
	*next_cwriter = gits_cwriter + 32;
}

static void its_wait_processed(void)
{
	unsigned long gits_creadr, gits_cwriter;

	while (1) {
		read_memory(its_addr + GITS_CREADR, &gits_creadr, sizeof(gits_creadr));
		read_memory(its_addr + GITS_CWRITER, &gits_cwriter, sizeof(gits_cwriter));

		if (gits_creadr == gits_cwriter)
			break;
		else if (gits_creadr & 0b1) {
			fprintf(stderr, ">> stalled cmdq\n");
			write_memory(its_addr + GITS_CWRITER, &gits_creadr, sizeof(gits_creadr));
			return;
		}

	}
}

void its_send_mapd(unsigned int dev_id, unsigned long content)
{
	unsigned long next_cmd_pa, next_cwriter;

	get_next_command(&next_cmd_pa, &next_cwriter);

	struct its_cmd mapd = {
		.byte_0 = GITS_CMD_MAPD | ((unsigned long)dev_id << 32),
		.byte_1 = content & GENMASK_ULL(4,0),
		.byte_2 = content,
		.byte_3 = 0,
	};

	write_memory(next_cmd_pa, &mapd, sizeof(mapd));
	write_memory(its_addr + GITS_CWRITER, &next_cwriter, sizeof(next_cwriter)); 

	its_wait_processed();
}

void its_send_mapti(unsigned long content)
{
	unsigned long next_cmd_pa, next_cwriter;
	struct its_cmd mapti = {
		.byte_0 = GITS_CMD_MAPTI, /* only dev_id = 0 */
		.byte_1 = (content & GENMASK_ULL(63, 32)), /* only event_id = 0 */
		.byte_2 = content & GENMASK_ULL(15, 0),
		.byte_3 = 0,
	};

	get_next_command(&next_cmd_pa, &next_cwriter);

	write_memory(next_cmd_pa, &mapti, sizeof(mapti));
	write_memory(its_addr + GITS_CWRITER, &next_cwriter, sizeof(next_cwriter));

	its_wait_processed();
}

void its_send_mapc(unsigned long content)
{
	unsigned long next_cmd_pa, next_cwriter;
	struct its_cmd mapc = {
		.byte_0 = GITS_CMD_MAPC, /* only dev_id = 0 */
		.byte_1 = 0,
		.byte_2 = (content & GENMASK_ULL(50, 16)) | (1UL << 63), /* icid 0 */
		.byte_3 = 0,
	};

	get_next_command(&next_cmd_pa, &next_cwriter);

	write_memory(next_cmd_pa, &mapc, sizeof(mapc));
	write_memory(its_addr + GITS_CWRITER, &next_cwriter, sizeof(next_cwriter));

	its_wait_processed();
}

void its_send_vmapp(unsigned long content)
{
	unsigned long next_cmd_pa, next_cwriter;
	struct its_cmd vmapp = {
		.byte_0 = GITS_CMD_VMAPP | (3 << 8) | (content & GENMASK_ULL(51, 16)),
		.byte_1 = 1023,/* vpeID 0, no doorbell */
		.byte_2 = (1UL << 63),  /* Redistributor */
		.byte_3 = (content & GENMASK_ULL(51, 16)) | 0xf,
	};

	get_next_command(&next_cmd_pa, &next_cwriter);

	write_memory(next_cmd_pa, &vmapp, sizeof(vmapp));
	write_memory(its_addr + GITS_CWRITER, &next_cwriter, sizeof(next_cwriter));

	its_wait_processed();
}

void its_set_power(int enable)
{
	int gits_ctlr = 0;
	write_memory(its_addr, &enable, sizeof(enable));
	do {
		read_memory(its_addr, &gits_ctlr, sizeof(gits_ctlr));
	} while ((gits_ctlr & (1 << 31)) == enable);
}

void its_send_vsync(unsigned long vpeId)
{
	unsigned long next_cmd_pa, next_cwriter;
	struct its_cmd vmapp = {
		.byte_0 = GITS_CMD_VSYNC,
		.byte_1 = vpeId << 32,
		.byte_2 = 0,
		.byte_3 = 0,
	};

	get_next_command(&next_cmd_pa, &next_cwriter);

	write_memory(next_cmd_pa, &vmapp, sizeof(vmapp));
	write_memory(its_addr + GITS_CWRITER, &next_cwriter, sizeof(next_cwriter));

	its_wait_processed();
}

void its_send_vinvall(unsigned long vpeId)
{
	unsigned long next_cmd_pa, next_cwriter;
	struct its_cmd vinv = {
		.byte_0 = GITS_CMD_VINVALL,
		.byte_1 = vpeId << 32,
		.byte_2 = 0,
		.byte_3 = 0,
	};

	get_next_command(&next_cmd_pa, &next_cwriter);

	write_memory(next_cmd_pa, &vinv, sizeof(vinv));
	write_memory(its_addr + GITS_CWRITER, &next_cwriter, sizeof(next_cwriter));

	its_wait_processed();
}
