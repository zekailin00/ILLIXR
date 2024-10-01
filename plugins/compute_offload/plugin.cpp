#include "illixr/data_format.hpp"
#include "illixr/managed_thread.hpp"
#include "illixr/phonebook.hpp"
#include "illixr/relative_clock.hpp"
#include "illixr/switchboard.hpp"
#include "illixr/threadloop.hpp"
#include "illixr/pose_prediction.hpp"

#include <stdbool.h>
#include <unistd.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <stdint.h>

// #define FIRESIM
#define SOCKET
#include "bridge.h"
#include "offload_protocol.h"

#include <chrono>


#define STATUS_CHECK(status, msg) if (status)   \
    {                                           \
        perror("ERROR: " #msg "\n");            \
        throw;                                  \
    }

#define LOG(msg) do {           \
        printf("%s\n", msg);    \
    } while(0)
// #define LOG(msg)

#define MIN(X, Y) (((X) < (Y)) ? (X) : (Y))

#define IMG_WIDTH   56
#define IMG_HEIGHT  56
#define IMG_CHN     1


using namespace ILLIXR;

class compute_offload: public ILLIXR::threadloop
{
public:
    compute_offload(const std::string& name_, phonebook* pb_):
        threadloop{name_, pb_},
        _m_sb{pb->lookup_impl<switchboard>()},
        pp{pb->lookup_impl<pose_prediction>()},
        offloadImageTopic{_m_sb->get_writer<unsigned char*>("offloadImage")}
        {
            STATUS_CHECK(initializeBridge(), "Failed to initialize bridge.");
        }

protected:
    void _p_one_iteration() override
    {

        switch (packet_arrived()) 
        {
            case MMIO_type: {
                printf("MMIO request received");
                uint32_t cmd = ROSE_RX_DATA_1;
                if (cmd == CS_REQ_POSE) 
                {
                    fast_pose_type fastPose = pp->get_fast_pose(); 
                    float buffer[7] = {
                        fastPose.pose.orientation.x(),
                        fastPose.pose.orientation.y(),
                        fastPose.pose.orientation.z(),
                        fastPose.pose.orientation.w(),
                        fastPose.pose.position.x(),
                        fastPose.pose.position.y(),
                        fastPose.pose.position.z(),
                    };

                    printf("[firesim-target] Sending Pose...\n");

                    while ((reg_read8(ROSE_STATUS_ADDR) & 0x1) == 0) ;
                    reg_write32(ROSE_TX_DATA_ADDR, CS_RSP_POSE);
                    while ((reg_read8(ROSE_STATUS_ADDR) & 0x1) == 0) ;
                    reg_write32(ROSE_TX_DATA_ADDR, 7*4);

                    for (int i = 0; i < 7; i++) {
                        while ((reg_read8(ROSE_STATUS_ADDR) & 0x1) == 0) ;
                        reg_write32(ROSE_TX_DATA_ADDR, *((uint32_t *) &buffer[i]));
                    }

                    while ((reg_read8(ROSE_STATUS_ADDR) & 0x1) == 0) ;
                    reg_write32(ROSE_TX_DATA_ADDR, 0);

                    printf("[firesim-target] Sent Pose...\n");

                } 
                else
                {
                    printf("unknown packet command: %x\n", cmd);
                } 
                break;
            }
            case DMA_type: {

                printf("reading dma image");
                uint8_t *pointer;
                pointer = (uint8_t *) ROSE_DMA_BASE_ADDR_0 + 56*56*4;  
                memcpy(imageBuffer, pointer, 56*56*4);

                for (int i = 0; i < 56; i++) {
                    for (int j = 0; j < 56; j++) {
                        printf("%d ", imageBuffer[i*56+j]);
                    }
                    printf("\n");
                }

                break;
            }
            default:
                printf("no packets\n");
                break;

        }


        // message_packet_t packet;
        // if (packet_arrived())
        // {
        //     LOG("DEBUG: new data is arriving");

        //     ssize_t received = receive(&packet.header, sizeof(header_t));
        //     STATUS_CHECK(received != sizeof(header_t), "Error receiving header");

        //     if (packet.header.payload_size != 0)
        //     {
        //         int payload_size = packet.header.payload_size;
        //         packet.payload = (char *) malloc(payload_size);
        //         STATUS_CHECK(packet.payload == NULL, "Error allocating memory for message data");
            
        //         size_t bytes_received = 0;
        //         while (bytes_received < payload_size)
        //         {
        //             size_t chunk_size = MIN(1024ul, payload_size - bytes_received);
        //             char* data_ptr = packet.payload + bytes_received;
        //             ssize_t chunk_bytes_received = receive(data_ptr, chunk_size);

        //             STATUS_CHECK(chunk_bytes_received == -1, "Error receiving message data");
        //             bytes_received += chunk_bytes_received;
        //         }
        //     }

        //     if (packet.header.command == CS_REQ_POSE)
        //     {
        //         fast_pose_type fastPose = pp->get_fast_pose(); 
        //         float buffer[7] = {
        //             fastPose.pose.orientation.x(),
        //             fastPose.pose.orientation.y(),
        //             fastPose.pose.orientation.z(),
        //             fastPose.pose.orientation.w(),
        //             fastPose.pose.position.x(),
        //             fastPose.pose.position.y(),
        //             fastPose.pose.position.z(),
        //         };

        //         assert(sizeof(buffer) == 28);
        //         packet.header.command = CS_RSP_POSE;
        //         packet.header.payload_size = sizeof(buffer);
        //         packet.payload = (char*)buffer;

        //         size_t size_sent = send(&packet, sizeof(header_t));
        //         STATUS_CHECK(size_sent != sizeof(header_t), "DEBUG: failed to send header");

        //         if (packet.header.payload_size != 0)
        //         {
        //             size_t index = 0;
        //             while (index < packet.header.payload_size)
        //             {
        //                 size_t chunk_size = MIN(1024ul, packet.header.payload_size - index);
        //                 const char* data_ptr = packet.payload + index;
        //                 ssize_t bytes_sent = send(data_ptr, chunk_size);

        //                 LOG("DEBUG: sent pose");
        //                 STATUS_CHECK(bytes_sent == -1, "DEBUG: failed to send everything");
        //                 index += bytes_sent;
        //             }
        //         }
        //     }
        //     else if (packet.header.command == CS_RSP_IMG)
        //     {
        //         static int imageIndex = 0;
        //         memcpy(imageBuffer, packet.payload, packet.header.payload_size);
        //         // offloadImageTopic.put(offloadImageTopic.allocate<unsigned char*>(imageBuffer));
        //         free(packet.payload);
        //         printf("received image: %d\n", imageIndex++);
        //     }
        //     else if (packet.header.command == CS_GRANT_TOKEN)
        //     {
        //         printf("Token granted\n");
        //     }
        //     else if (packet.header.command == CS_DEFINE_STEP)
        //     {
        //         printf("Step Defined: %d\n", *((int*)packet.payload));
        //     }
        // }

    }

private:
    const std::shared_ptr<switchboard> _m_sb;
    const std::shared_ptr<pose_prediction> pp;
    switchboard::writer<unsigned char*> offloadImageTopic;

    // unsigned char imageBuffer[IMG_WIDTH * IMG_HEIGHT * IMG_CHN];
    uint32_t imageBuffer[IMG_WIDTH * IMG_HEIGHT * IMG_CHN];
};

PLUGIN_MAIN(compute_offload)
