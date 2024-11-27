#pragma once

#include "offload_protocol.h"

bool packet_arrived();
int initializeBridge();
int receive(void* buf, unsigned int bytes);
int send(const void* buf, unsigned int bytes);


// #define FIRESIM
// #define SOCKET

#if defined(SOCKET)

#include <netinet/in.h>
#include <sys/socket.h>
#include <arpa/inet.h>
#include <stdio.h>

static int client_fd = -1;
#define PORT    8080
#define IP_ADDR "127.0.0.1"

#define SOCKET_CHECK(status) if (status < 0)    \
    {                                           \
        perror("Failed: " #status "\n");        \
        exit(EXIT_FAILURE);                     \
    }


int initializeBridge()
{
    SOCKET_CHECK((client_fd = socket(AF_INET, SOCK_STREAM, 0)) < 0);

    struct sockaddr_in address = {};
    address.sin_family = AF_INET;
    address.sin_port = htons(PORT);
 
    // Convert IPv4 and IPv6 addresses from text to binary form
    SOCKET_CHECK(inet_pton(AF_INET, IP_ADDR, &address.sin_addr) <= 0);
    int result = connect(client_fd, (struct sockaddr*)&address, sizeof(address));

    printf("Socket connected: %d\n", result);
    
    return result;
}

bool packet_arrived()
{
    message_packet_t packet;
    ssize_t bytes = recv(
        client_fd, &packet.header, sizeof(header_t),
        MSG_PEEK | MSG_DONTWAIT
    );
    bool result = false;
    if (bytes > 0)
    {
        result = true;
    }
    return result;
}

int receive(void* buf, unsigned int bytes)
{
    int c = bytes;
    unsigned char* cbuf = (unsigned char*)buf;
    do {
        int bytesRecv = recv(client_fd, cbuf, bytes, 0);
        assert(bytesRecv != -1);
        cbuf += bytesRecv;
        bytes -=bytesRecv;

    } while(bytes > 0);
    return c;
}

int send(const void* buf, unsigned int bytes)
{
    return send(client_fd, buf, bytes, 0);
}

#elif defined(FIRESIM)


bool packet_arrived()
{
    return 0;
}

int initializeBridge()
{
    return 0;
}

int receive(void* buf, unsigned int bytes)
{
    return 0;
}

int send(const void* buf, unsigned int bytes)
{
    return 0;
}

#else // emulated interface

bool packet_arrived()
{
    static int count = 0;
    if (count++ == 10000)
    {
        count = 0;
        return true;
    }

    return false;
}

int initializeBridge()
{
    return 0;
}

int receive(void* buf, unsigned int bytes)
{
    header_t pack {
        .command = CS_REQ_POSE,
        .payload_size = 0
    };

    assert(bytes >= sizeof(pack));
    memcpy(buf, &pack, sizeof(pack));
    return sizeof(pack);
}

int send(const void* buf, unsigned int bytes)
{
    static bool sentHeader = false;
    if (bytes == sizeof(header_t))
    {
        assert(!sentHeader);
        sentHeader = true;
    }
    else if(sentHeader)
    {
        assert(bytes == 28);
        sentHeader = false;

        float* data = (float*) buf;

        printf("[Bridge] data: <%f, %f, %F, %f>, <%f, %f, %f>\n",
            data[0], data[1], data[2], data[3], data[4], data[5], data[6]);

    }
    printf("[Bridge] send pack of size %d\n", bytes);
    return bytes;
}


#endif