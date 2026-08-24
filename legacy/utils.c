#include "ft_ping.h"

void	fatal_error(char *msg)
{
	dprintf(2, "%s\n", msg);
	exit(1);
}
