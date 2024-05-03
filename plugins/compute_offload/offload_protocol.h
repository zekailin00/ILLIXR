#pragma once

#include <stdint.h>

enum socket_protocol
{
    CS_GRANT_TOKEN = 0x80,
    CS_DEFINE_STEP = 0x83,
    CS_RSP_STALL   = 0x84,

    CS_REQ_POSE = 1u,
    CS_RSP_POSE = 2u,
    CS_RSP_IMG  = 3u,

};

typedef struct header
{
    uint32_t command;
    uint32_t payload_size;
} header_t;

typedef struct message_packet
{
  header_t header;
  char *payload;
} message_packet_t;