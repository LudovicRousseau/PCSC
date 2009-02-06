#include <stdio.h>

int pcsc_assert_wire_constants(void);

int
main (/*@unused@*/ int argc, /*@unused@*/ char **argv)
{
	(void)argc;
	(void)argv;

	printf("Checking ABI...\n");
    if (0 == pcsc_assert_wire_constants ())
		printf("OK\n");
    return 0;
}
