/*
 * Busyloop count utility for speed testing
 *
 * (C) Copyright 2021, iVeia, LLC
 */
#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>
#include <fcntl.h>

int main(int argc, char **argv)
{
	signed long long count;
	
	if(argc != 2) {
		fprintf(stderr,
            "Summary: Busyloop for given <count>\n"
            "Usage: %s <count>\n"
            "count: Times to loop.  Signed long long.  If negative, run forever.\n",
			argv[0]);
		exit(1);
	}

	count = strtoull(argv[1], NULL, 0);
    if (count < 0) {
        for (;;);
    } else {
        for (;count--;);
    }

    return 0;
}

