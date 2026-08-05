#include <sys/socket.h>
#include <stdio.h>

int main(int ac, char *av[])
{
	int sockfd = 0;

	sockfd = socket(AF_INET, SOCK_RAW, IPPROTO_ICMP);
	if (sockfd < 0)
	{
		perror("Scoket error\n");
		exit(1);
	}
	return 0;
}
