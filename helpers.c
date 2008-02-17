
#include <string.h>
#include <sys/stat.h>
#include <sys/types.h>
#include <fcntl.h>
#include <unistd.h>
#include <sys/ioctl.h>
#include <sys/socket.h>
#include <net/if.h>
#include <netinet/in.h>
#include <arpa/inet.h>

#include "helpers.h"

/*
 * Helpers to replace system() calls
 */

int makepath(char *p)
{
	char *x, path[PATH_MAX];
	int ret;
	
	x = path;

	do {
		do { *x++ = *p++; } while (*p && *p != '/');
		ret = mkdir(path, 0777);
	} while (*p && (*p != '/' || *(p + 1))); /* ignore trailing slash */

	return ret;
}

void ifconfig(char *name, char *inet, char *mask, int up)
{
	struct ifreq ifr;
	struct sockaddr_in *a = (struct sockaddr_in *)&(ifr.ifr_addr);
	int sock;

	if ((sock = socket(PF_INET, SOCK_DGRAM, IPPROTO_IP)) < 0)
		return;

	memset(&ifr, 0, sizeof (ifr));
	strncpy(ifr.ifr_name, name, IFNAMSIZ);
	a->sin_family = AF_INET;
	inet_aton(inet, &a->sin_addr);

	if (up) {
		ioctl(sock, SIOCSIFADDR, &ifr);
		inet_aton(mask, &a->sin_addr);
		ioctl(sock, SIOCSIFNETMASK, &ifr);
		ioctl(sock, SIOCGIFFLAGS, &ifr);
		ifr.ifr_flags |= IFF_UP;
		ioctl(sock, SIOCSIFFLAGS, &ifr);
	} else {
		ioctl(sock, SIOCGIFFLAGS, &ifr);
		ifr.ifr_flags &= ~IFF_UP;
		ioctl(sock, SIOCSIFFLAGS, &ifr);
	}
	
	close(sock);
}

#define BUF_SIZE 4096

void copyfile(char *src, char *dst, int size)
{
	char buffer[BUF_SIZE];
	int s, d, n;

	if ((s = open(src, O_RDONLY)) >= 0) {
		if ((d = open(dst, O_WRONLY | O_CREAT, 0644)) >= 0) {

			/* Size == 0 means copy entire file */
			if (size == 0) {
				do {
					if ((n = read(s, buffer, BUF_SIZE)) > 0)
						write(d, buffer, n);
				} while (n == BUF_SIZE);
			} else {
				do {
					int csize = size > BUF_SIZE ?
							BUF_SIZE : size;
			
					if ((n = read(s, buffer, csize)) > 0)
						write(d, buffer, n);
					size -= csize;
				} while (size > 0 && n == BUF_SIZE);
			}
			close(d);
		}
		close(s);
	}
}

