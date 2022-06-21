/*
 * Dump bytes from /dev/mem to stdout
 *
 * Existing utilities have some issues:
 *  - The devmem utility only supports a single access, and always converts to
 *    text output.
 *  - The dd utility does not use mmap(), and therefore fails with Bad address
 *    when reading /dev/mem
 *
 * This simple utility solves those issues.
 *
 * (C) Copyright 2021, iVeia, LLC
 */
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <fcntl.h>
#include <zlib.h>
#include <sys/types.h>
#include <sys/mman.h>
#include <arpa/inet.h>

#define MAGIC 0x4a544147

struct header {
    unsigned int magic;
    unsigned int len;
    unsigned int be_crc32;  // CRC32 in big endian (network) format
};

int main(int argc, char **argv)
{
    int fd;
    void * map_base;
    unsigned char * payload;
	off_t start;
	unsigned int len;
    struct header header;
    int has_header = 0;
	
	if(argc != 3) {
		fprintf(stderr,
            "Summary: Dump bytes from /dev/mem to stdout\n"
            "\n"
            "Usage:\n"
            "    %s <start_addr> <num_bytes>\n"
            "    %s -H <start_addr>\n"
            "\n"
            "Arguments:\n"
            "    start_addr: Start address to dump.  Decimal or hex.  Must be page aligned.\n"
            "    num_bytes: Number of bytes to dump.  Decimal or hex. \n"
            "\n"
            "Returns: Zero on success.\n"
            "\n"
            "Notes:\n"
            "   If -H is specified, then the start_addr begins with a 3 32-bit word header\n"
            "   with the following fields:\n"
            "     - MAGIC: Magic number header identifier\n"
            "     - LEN: Number of bytes that follow this header\n"
            "     - CRC32: CRC32 of LEN bytes following header\n"
            "   In this case, the number of bytes to dump will be determined from the header\n"
            "   and the CRC32 will be verified.\n"
            "\n"
            "   The start_address must be a region outside of Linux addressable memory, but\n"
            "   still within physical RAM.  Generally, only useful if the Linux addressable\n"
            "   memory was artifically limited with the mem=NNN kernel command line option.\n"
            "   Using an address outside of physical RAM will crash Linux.\n"
            "\n",
			argv[0],
			argv[0]);
		exit(1);
	}

    if (strcmp("-H", argv[1]) == 0) {
        start = strtoul(argv[2], NULL, 0);

        if ((fd = open("/dev/mem", O_RDONLY)) == 1) return 1;

        map_base = mmap(NULL, sizeof(struct header), PROT_READ, MAP_SHARED, fd, start);
        if (map_base == MAP_FAILED) return 2;

        memcpy((void *) &header, map_base, sizeof(struct header));

        close(fd);

        if (header.magic != MAGIC) return 3;
        len = header.len;
        has_header = 1;

    } else {
        start = strtoul(argv[1], NULL, 0);
        len = strtoul(argv[2], NULL, 0);
    }

    if ((fd = open("/dev/mem", O_RDONLY)) == 1) return 4;

    unsigned int map_len = has_header ? len + sizeof(struct header) : len;
    map_base = mmap(NULL, map_len, PROT_READ, MAP_SHARED, fd, start);
    if (map_base == MAP_FAILED) return 5;

    payload = (unsigned char *) map_base;
    if (has_header) {
        payload += sizeof(struct header);
        unsigned int computed_crc32 = crc32(0, payload, len);
        if (computed_crc32 != ntohl(header.be_crc32)) return 6;
    }

    for (unsigned int i = 0; i < len; i++) {
        putchar(payload[i]);
    }

	if (munmap(map_base, len) == -1) return 7;
    close(fd);

    return 0;
}

