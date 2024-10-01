#pragma once

#include "offload_protocol.h"

reqrsp_type packet_arrived();
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

#include <fcntl.h>
#include <sys/mman.h>
#include <unistd.h>
#include "rose_port.h"
#include "mmio.h"

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
    // SOCKET_CHECK((client_fd = socket(AF_INET, SOCK_STREAM, 0)) < 0);

    // struct sockaddr_in address = {};
    // address.sin_family = AF_INET;
    // address.sin_port = htons(PORT);
 
    // // Convert IPv4 and IPv6 addresses from text to binary form
    // SOCKET_CHECK(inet_pton(AF_INET, IP_ADDR, &address.sin_addr) <= 0);
    // SOCKET_CHECK((connect(client_fd, (struct sockaddr*)&address, sizeof(address))) < 0);


    printf("Trying to mmap addresses\n");
    int mem_fd;
    mem_fd = open("/dev/mem", O_RDWR | O_SYNC);
    ptr = (intptr_t) mmap(NULL, 24, PROT_READ | PROT_WRITE, MAP_SHARED, mem_fd, 0x2000);
    printf("Ptr: %lx\n", ptr);

    
    int mem_fd2;
    mem_fd2 = open("/dev/mem", O_RDWR | O_SYNC);
    dma_ptr = (intptr_t) mmap(NULL, 56*56*4, PROT_READ | PROT_WRITE, MAP_SHARED, mem_fd2, 0x88000000);
    printf("dma ptr: %lx\n", dma_ptr);

    printf("[firesim-target] configuring counter\n");
    reg_write32(ROSE_DMA_CONFIG_COUNTER_ADDR_0, 56*56*4);

    return 0;
}

reqrsp_type packet_arrived()
{
    // message_packet_t packet;
    // ssize_t bytes = recv(
    //     client_fd, &packet.header, sizeof(header_t),
    //     MSG_PEEK | MSG_DONTWAIT
    // );
    // bool result = false;
    // if (bytes > 0)
    // {
    //     result = true;
    // }
    // return result;

    uint8_t status = reg_read32(ROSE_STATUS_ADDR);

    if (status & 0x2) {
        return MMIO_type;
    } else if (status & 0x4) {
        return DMA_type;
    } else {
        return NONE_type;
    }

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


reqrsp_type packet_arrived()
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

reqrsp_type packet_arrived()
{
    return NONE;
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