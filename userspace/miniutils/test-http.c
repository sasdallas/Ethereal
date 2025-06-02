#include <sys/socket.h>
#include <arpa/inet.h>
#include <netinet/in.h>
#include <stdio.h>
#include <netdb.h>
#include <unistd.h>
#include <string.h>


int main(int argc, char *argv[]) {
	if (argc < 2) {
		printf("usage: test-http <addr>\n");
		return 1;
	}

    	struct hostent *ent = gethostbyname(argv[1]);
    	if (!ent) {
        	printf("test-http: %s: not found by DNS\n", argv[1]);
        	return 1;
    	}
	
	struct in_addr *addrinfo = (struct in_addr*)ent->h_addr_list[0];
	
	int sock = socket(AF_INET, SOCK_STREAM, IPPROTO_TCP);
	if (sock < 0) {
		perror("socket");
		return 1;
	}

	struct sockaddr_in in = {
		.sin_addr.s_addr = addrinfo->s_addr,
		.sin_port = htons(80),
		.sin_family = AF_INET
	};
	
	if (connect(sock, (const struct sockaddr*)&in, sizeof(struct sockaddr_in)) < 0) {
		perror("connect");
		return 1;
	}

	
	char *data = "GET / HTTP/1.1\r\nHost: www.example.com\r\nConnection: close\r\n\r\n";

	if (send(sock, data, strlen(data), 0) < 0) {
		perror("send");
		return 1;
	}

	char resp[1024];
	if (recv(sock, resp, 1024, 0) < 0) {
		perror("recv");
		return 1;
	}

	printf("RESPONSE: %s\n", resp);
	close(sock);
	return 0;
}
