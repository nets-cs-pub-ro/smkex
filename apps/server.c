#include "util.h"

#include <stdlib.h>
#include <stdio.h>
#include <netinet/in.h>
#include <sys/types.h>
#include <sys/socket.h>
#include <arpa/inet.h>
#include <sys/stat.h>
#include <fcntl.h>
#include <unistd.h>
#include <getopt.h>
#include <string.h>
#include <sys/stat.h>

#define DEFAULT_IP   "127.0.0.1"
#define DEFAULT_PORT 1337

#define BUFFER_SIZE  4096

#define SO_SMKEX_DHONLY 0xA002

int my_send(int sockfd, char * buffer, int length) {
    int bytes_sent = 0;
    int rc;
    while (bytes_sent < length) {
        rc = send(sockfd, buffer + bytes_sent, length - bytes_sent, 0);
        CHECK(rc >= 0, "send");

        bytes_sent += rc;
    }

    return bytes_sent;
}

int main(int argc, char* argv[]) {
    unsigned int serv_port = DEFAULT_PORT;
    char* serv_ip = DEFAULT_IP;
    char* filename = "smallfile.dat";
    struct stat file_stat;
    char buf[BUFFER_SIZE];
    int rc;
    char dh_only = 0;

    int opt;
    while ((opt = getopt(argc, argv, "i:p:f:d")) != -1) {
        switch (opt) {
            case 'i':
                serv_ip = optarg;
                break;
            case 'p':
                serv_port = atoi(optarg);
                break;
            case 'f':
                filename = optarg;
                break;
            case 'd':
                dh_only = 1;
                break;
            default:
                fprintf(stderr, "Usage %s [-i IP] [-p PORT] [-f FILENAME] [-d]\n", argv[0]);
                exit(EXIT_FAILURE);
        }
    }

    /* Create new socket */
    int listen_fd = socket(AF_INET, SOCK_STREAM, 0);
    CHECK(listen_fd >= 0, "socket");

    // Setup sockaddr_in struct
    struct sockaddr_in server_addr;
    server_addr.sin_family = AF_INET;
    server_addr.sin_addr.s_addr = inet_addr(serv_ip);
    server_addr.sin_port = htons(serv_port);

    // Bind
    rc = bind(listen_fd, (const struct sockaddr*)&server_addr, sizeof(server_addr));
    CHECK(rc >= 0, "bind");

    // Listen
    rc = listen(listen_fd, 0);
    CHECK(rc >= 0, "listen");

    printf("[server] Server listening on port %d...\n", serv_port);

    // Set only DH if requested
    if (dh_only)
    {
      printf("[server] Setting DH ONLY for socket %d\n", listen_fd);
      rc = setsockopt(listen_fd, SOL_SOCKET, SO_SMKEX_DHONLY, NULL, 0);
      CHECK(rc == 0, "setsockopt");
    }

    // Open file
    int file_fd = open(filename, O_RDONLY);
    CHECK(file_fd >= 0, "open");
    rc = fstat(file_fd, &file_stat);
    CHECK(rc >= 0, "fstat");

    while (1) {
        struct sockaddr_in client_addr;
        socklen_t client_len;

        printf("[server] Before accept()\n");
        int connect_fd = accept(listen_fd, (struct sockaddr*)&client_addr, &client_len);
        CHECK(connect_fd >= 0, "accept");
        printf("[server] Got a request...\n");

        ///*
        uint32_t file_size = htonl((uint32_t)file_stat.st_size);
        printf("[server] Sending file size (%zd)...\n", file_stat.st_size);
        rc = my_send(connect_fd, (char*)&file_size, sizeof(file_size));
        CHECK(rc >= 0, "send");

        while (1) {
            int bytes_read = read(file_fd, buf, BUFFER_SIZE);
            CHECK(bytes_read >= 0, "read");

            if (bytes_read == 0)
                break;

#if DEBUG
            printf("[dbg][server] read %d bytes from file, sending to client...\n", bytes_read);
#endif

            my_send(connect_fd, buf, bytes_read);
        }
        lseek(file_fd, 0, SEEK_SET);
        printf("[server] File send completed\n");
        //*/

        close(connect_fd);
        printf("[server] Connection closed on socket %d...\n", connect_fd);
    }

    close(listen_fd);
    close(file_fd);

    return 0;
}
