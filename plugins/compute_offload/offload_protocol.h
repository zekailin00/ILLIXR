#pragma once

#include <stdint.h>

enum socket_protocol
{
    CS_GRANT_TOKEN = 0x80,
    CS_DEFINE_STEP = 0x83,
    CS_RSP_STALL   = 0x84,

    CS_REQ_POSE = 0x10,
    CS_RSP_POSE = 0x11,
    CS_RSP_IMG  = 0x12,

};

enum reqrsp_type
{
  NONE_type = 0x0,
  MMIO_type = 0x2,
  DMA_type = 0x4,
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