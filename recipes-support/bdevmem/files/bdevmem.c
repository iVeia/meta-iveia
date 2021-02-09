/*
 * Dump bytes from /dev/mem to stdout
 *
 * Existing utilities have some issues:
 *  - The devmem utility only supports single byte accesses, and always
 *    converts to text output.
 *  - The dd utility does not use mmap(), and therefore fails with Bad address
 *    when reading /dev/mem
 *
 * This simple utility solves those issues.
 *
 * (C) Copyright 2021, iVeia, LLC
 */
#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>
#include <fcntl.h>
#include <sys/types.h>
#include <sys/mman.h>

int main(int argc, char **argv)
{
    int fd;
    void *map_base;
	off_t start;
	unsigned int len;
	
	if(argc != 3) {
		fprintf(stderr,
            "Summary: Dump bytes from /dev/mem to stdout\n"
            "Usage: %s <start_addr> <num_bytes>\n"
            "start_addr: Start address to dump.  Must be page aligned.\n"
            "num_bytes: Number of bytes to dump.\n",
			argv[0]);
		exit(1);
	}
	start = strtoul(argv[1], NULL, 0);
	len = strtoul(argv[2], NULL, 0);

    if ((fd = open("/dev/mem", O_RDONLY)) == 1) return 1;

    map_base = mmap(NULL, len, PROT_READ, MAP_SHARED, fd, start);
    if (map_base == MAP_FAILED) return 2;

    for (unsigned int i = 0; i < len; i++) {
        putchar(*((unsigned char *) (map_base + i)));
    }

	if (munmap(map_base, len) == -1) return 3;
    close(fd);

    return 0;
}

