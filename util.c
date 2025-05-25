// Big thanks to https://www.gabriel.urdhr.fr/2021/05/08/tuntap/ for some code examples we reused here!

#include <sys/types.h>
#include <sys/stat.h>
#include <fcntl.h>
#include <string.h>
#include <unistd.h>
#include <stdio.h>
#include <netdb.h>
#include <stdlib.h>
#include <stdbool.h>
#include <sys/wait.h>
#include <signal.h>

#include <netinet/in.h>	  // IPPROTO_*
#include <net/if.h>	  // ifreq
#include <linux/if_tun.h> // IFF_TUN, IFF_NO_PI

#include <sys/ioctl.h>

#define BUFFLEN (4 * 1024)

const char HEX[] = {
	'0',
	'1',
	'2',
	'3',
	'4',
	'5',
	'6',
	'7',
	'8',
	'9',
	'a',
	'b',
	'c',
	'd',
	'e',
	'f',
};

void hex(char *source, char *dest, ssize_t count)
{
	for (ssize_t i = 0; i < count; ++i) {
		unsigned char data = source[i];
		dest[2 * i] = HEX[data >> 4];
		dest[2 * i + 1] = HEX[data & 15];
	}

	dest[2 * count] = '\0';
}

int has_ports(int protocol)
{
	switch (protocol) {
	case IPPROTO_UDP:
	case IPPROTO_UDPLITE:
	case IPPROTO_TCP:
		return 1;
	default:
		return 0;
	}
}

void dump_ports(int protocol, int count, const char *buffer)
{
	if (!has_ports(protocol))
		return;
	if (count < 4)
		return;
	uint16_t source_port;
	uint16_t dest_port;

	memcpy(&source_port, buffer, 2);
	source_port = htons(source_port);
	memcpy(&dest_port, buffer + 2, 2);
	dest_port = htons(dest_port);

	fprintf(stderr, " sport=%u, dport=%d\n", (unsigned)source_port, (unsigned)dest_port);
}

// For some "magic numbers" here and below - see https://en.wikipedia.org/wiki/IPv4
void dump_packet_ipv4(int count, char *buffer)
{
	char buffer2[2 * BUFFLEN + 1];

	if (count < 20) {
		fprintf(stderr, "IPv4 packet too short\n");
		return;
	}

	hex(buffer, buffer2, count);

	int protocol = (unsigned char)buffer[9];
	struct protoent *protocol_entry = getprotobynumber(protocol);

	unsigned ttl = (unsigned char)buffer[8];

	fprintf(stderr, "IPv4: src=%u.%u.%u.%u dst=%u.%u.%u.%u proto=%u(%s) ttl=%u\n",
		(unsigned char)buffer[12], (unsigned char)buffer[13], (unsigned char)buffer[14], (unsigned char)buffer[15],
		(unsigned char)buffer[16], (unsigned char)buffer[17], (unsigned char)buffer[18], (unsigned char)buffer[19],
		(unsigned)protocol,
		protocol_entry == NULL ? "?" : protocol_entry->p_name, ttl);

	dump_ports(protocol, count - 20, buffer + 20);
}

// For some "magic numbers" here and below - see https://en.wikipedia.org/wiki/IPv6
void dump_packet_ipv6(int count, char *buffer)
{
	char source_address[33];
	char destination_address[33];
	char buffer2[2 * BUFFLEN + 1];

	if (count < 40) {
		fprintf(stderr, "IPv6 packet too short\n");
		return;
	}

	hex(buffer, buffer2, count);

	int protocol = (unsigned char)buffer[6];
	struct protoent *protocol_entry = getprotobynumber(protocol);

	hex(buffer + 8, source_address, 16);
	hex(buffer + 24, destination_address, 16);

	int hop_limit = (unsigned char)buffer[7];

	fprintf(stderr, "IPv6: src=%s dst=%s proto=%u(%s) hop_limit=%i\n",
		source_address, destination_address,
		(unsigned)protocol,
		protocol_entry == NULL ? "?" : protocol_entry->p_name,
		hop_limit);

	dump_ports(protocol, count - 40, buffer + 40);
}

void dump_packet(int count, char *buffer)
{
	unsigned char version = ((unsigned char)buffer[0]) >> 4;

	if (version == 4) {
		dump_packet_ipv4(count, buffer);
	} else if (version == 6) {
		dump_packet_ipv6(count, buffer);
	} else {
		fprintf(stderr, "Unknown packet version\n");
	}
}

// For some "magic numbers" here and below - see https://en.wikipedia.org/wiki/Ethernet_frame
void dump_ethernet_frame(int count, char *buffer)
{
	char buffer2[2 * BUFFLEN + 1];
	char source_address[13];
	char destination_address[13];

	hex(buffer, source_address, 6);
	hex(buffer + 6, destination_address, 6);

	hex(buffer, buffer2, count);
	fprintf(stderr, "debug %s \n", source_address);
	fprintf(stderr, "Ethernet: src mac=%s dst mac=%s\n", source_address, destination_address);
	fprintf(stderr, "HEX: %s\n", buffer2);
}

// For some "magic numbers" here and below - see https://en.wikipedia.org/wiki/IPv6
void make_icmp4_packet(char *buffer, char *output, int size)
{
	memcpy(output, buffer, size);
	// Emulate packet handling
	sleep(1);

	// Swapping src & dst addresses
	for (int i = 12; i <= 15; i++) {
		output[i] = buffer[i + 4];
	}

	for (int i = 16; i <= 19; i++) {
		output[i] = buffer[i - 4];
	}
}

// For some "magic numbers" here and below - see https://en.wikipedia.org/wiki/Neighbor_Discovery_Protocol
void make_icmp6_packet(char *buffer, char *output, int size)
{
	memcpy(output, buffer, size);
	// Emulate packet handling
	sleep(1);

	// Swapping src & dst addresses
	for (int i = 8; i <= 23; i++) {
		output[i] = buffer[i + 16];
	}

	for (int i = 24; i <= 39; i++) {
		output[i] = buffer[i - 16];
	}
}

void make_icmp_packet(char *buffer, char *output, int size)
{
	unsigned char version = ((unsigned char)buffer[0]) >> 4;
	if (version == 4) {
		make_icmp4_packet(buffer, output, size);
	} else if (version == 6) {
		make_icmp6_packet(buffer, output, size);
	} else {
		fprintf(stderr, "Unknown packet version\n");
	}
}

// For some "magic numbers" here and below - see https://en.wikipedia.org/wiki/Ethernet_frame
void make_ethernet_frame(char *buffer, char *output, int size)
{
	memcpy(output, buffer, 14);

	for (int i = 0; i < 6; i++) {
		output[i] = buffer[i + 6];
		output[i + 6] = buffer[i];
	}

	make_icmp_packet(buffer + 14, output + 14, size - 14);
}

// For some "magic numbers" here and below - see https://en.wikipedia.org/wiki/Ethernet_frame
void handle_ethernet_frame(int fd, ssize_t count, char *buffer)
{
	int res;
	char response[BUFFLEN];

	dump_ethernet_frame(count, buffer);

	dump_packet(count - 14, buffer + 14);

	make_ethernet_frame(buffer, response, count);

	res = write(fd, response, count);
	if (res == -1) {
		perror("write");
		exit(1);
	}
}

void handle_ip_packet(int fd, ssize_t count, char *buffer)
{
	int res;
	char response[BUFFLEN];

	dump_packet(count, buffer);

	make_icmp_packet(buffer, response, count);

	res = write(fd, response, count);
	if (res == -1) {
		perror("write");
		exit(1);
	}
}

void sa_hanlder(int signo, siginfo_t *info, void *context)
{
	pid_t pid;
	int status;

	while (true) {
		pid = waitpid(-1, &status, WNOHANG);
		if (pid <= 0)
			return;

		if (WIFEXITED(status) && WEXITSTATUS(status) != 0) {
			printf("Child exited with status %d\n", WEXITSTATUS(status));
		} else if (WIFSIGNALED(status)) {
			printf("Child received signal %d\n", WTERMSIG(status));
		}
	}
}

int main(int argc, char **argv)
{
	char *device_name;
	char *default_name = "sample-dev";
	int fd;
	struct ifreq ifr;
	short flags = IFF_TUN;
	int res;
	char buffer[BUFFLEN];
	ssize_t count;
	pid_t pid;
	bool isTap = false;
	struct sigaction act = { 0 };

	// Parse arguments
	device_name = malloc(strlen(default_name));
	strcpy(device_name, default_name);

	if (argc >= 2) {
		if (strlen(argv[1]) + 1 > IFNAMSIZ) {
			printf("Too long device name\n");
			return 1;
		}

		free(device_name);
		device_name = malloc(strlen(argv[1]) + 1);
		strcpy(device_name, argv[1]);
	}

	if (argc >= 3) {
		if (strcmp(argv[2], "tun") == 0) {
			flags = IFF_TUN;
		} else if (strcmp(argv[2], "tap") == 0) {
			flags = IFF_TAP;
			isTap = true;
		} else {
			printf("Unsupported device type %s\n", argv[2]);
			return 1;
		}
	}

	// Request a TUN / TAP device:
	fd = open("/dev/net/tun", O_RDWR);
	if (fd == -1) {
		perror("open");
		return 1;
	}

	memset(&ifr, 0, sizeof(ifr));

	ifr.ifr_flags = flags | IFF_NO_PI;
	strncpy(ifr.ifr_name, device_name, IFNAMSIZ);

	res = ioctl(fd, TUNSETIFF, &ifr);
	if (res == -1) {
		perror("ioctl");
		return 1;
	}

	// This prevents accumulating zombie processes
	act.sa_sigaction = &sa_hanlder;
	act.sa_flags = SA_RESTART;
	if (sigaction(SIGCHLD, &act, NULL) == -1) {
		perror("sigaction");
		return 1;
	}

	while (1) {
		// Read a IP packet / Ethernet frame and create a response packet / frame
		count = read(fd, buffer, BUFFLEN);
		if (count <= 0) {
			perror("read");
			return 1;
		}

		// We need fork() to handle each ping in a new process, otherwise we will accumulate ICMP packets
		// due to sleep() in packet handler
		pid = fork();
		if (pid < 0) {
			perror("fork");
			return 1;
		}

		if (pid == 0) {
			if (isTap) {
				handle_ethernet_frame(fd, count, buffer);
			} else {
				handle_ip_packet(fd, count, buffer);
			}
			return 0;
		}
	}

	free(device_name);
	return 0;
}
