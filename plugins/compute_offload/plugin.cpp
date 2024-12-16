#include "illixr/data_format.hpp"
#include "illixr/managed_thread.hpp"
#include "illixr/phonebook.hpp"
#include "illixr/relative_clock.hpp"
#include "illixr/switchboard.hpp"
#include "illixr/threadloop.hpp"
#include "illixr/pose_prediction.hpp"
#include "illixr/opencv_data_types.hpp"

#include <stdbool.h>
#include <unistd.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

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

#define IMG_WIDTH   1280
#define IMG_HEIGHT  720
#define IMG_CHN     4


using namespace ILLIXR;

class compute_offload: public ILLIXR::threadloop
{
public:
    compute_offload(const std::string& name_, phonebook* pb_):
        threadloop{name_, pb_},
        _m_sb{pb->lookup_impl<switchboard>()},
        pp{pb->lookup_impl<pose_prediction>()},
        imagePacket{_m_sb->get_writer<image_packet_type>("image_packet")},
        offloadImageTopic{_m_sb->get_writer<host_image_type>("host_image")},
        netThroughput{_m_sb->get_writer<network_throughput_type>("net_throughput")},
        _m_gaze{_m_sb->get_reader<gaze_type>("gaze")}
        {
            STATUS_CHECK(initializeBridge(), "Failed to initialize bridge.");
            timeStart = std::chrono::high_resolution_clock::now();
        }

protected:
    void _p_one_iteration() override
    {
        message_packet_t packet;
        if (packet_arrived())
        {
#ifndef NDEBUG
            LOG("DEBUG: new data is arriving");
#endif
            ssize_t received = receive(&packet.header, sizeof(header_t));
            STATUS_CHECK(received != sizeof(header_t), "Error receiving header");

            if (packet.header.payload_size != 0)
            {
                int payload_size = packet.header.payload_size;
                packet.payload = (char *) malloc(payload_size);
                STATUS_CHECK(packet.payload == NULL, "Error allocating memory for message data");
            
                size_t bytes_received = 0;
                while (bytes_received < payload_size)
                {
                    size_t chunk_size = MIN(1024ul, payload_size - bytes_received);
                    char* data_ptr = packet.payload + bytes_received;
                    ssize_t chunk_bytes_received = receive(data_ptr, chunk_size);

                    STATUS_CHECK(chunk_bytes_received == -1, "Error receiving message data");
                    bytes_received += chunk_bytes_received;
                }
            }

            byteReceived += (received + packet.header.payload_size);
            auto now = std::chrono::high_resolution_clock::now();
            auto elapsedTime = std::chrono::duration_cast<std::chrono::seconds>(now - timeStart).count();
            if (elapsedTime > 3)
            {
                netThroughput.put(
                    netThroughput.allocate<network_throughput_type>(
                        network_throughput_type{byteReceived / elapsedTime}
                    )
                );

                timeStart = now;
                byteReceived = 0;   
            }

            if (packet.header.command == CS_REQ_POSE)
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

                assert(sizeof(buffer) == 28);
                packet.header.command = CS_RSP_POSE;
                packet.header.payload_size = sizeof(buffer);
                packet.payload = (char*)buffer;

                size_t size_sent = send(&packet, sizeof(header_t));
                STATUS_CHECK(size_sent != sizeof(header_t), "DEBUG: failed to send header");

                if (packet.header.payload_size != 0)
                {
                    size_t index = 0;
                    while (index < packet.header.payload_size)
                    {
                        size_t chunk_size = MIN(1024ul, packet.header.payload_size - index);
                        const char* data_ptr = packet.payload + index;
                        ssize_t bytes_sent = send(data_ptr, chunk_size);

#ifndef NDEBUG
                        LOG("DEBUG: sent pose");
#endif
                        STATUS_CHECK(bytes_sent == -1, "DEBUG: failed to send everything");
                        index += bytes_sent;
                    }
                }
            }
            else if (packet.header.command == CS_RSP_IMG)
            {
                std::vector<uint8_t> image;
                image.resize(packet.header.payload_size);
                memcpy(image.data(), packet.payload, packet.header.payload_size);

                // offloadImageTopic.put(
                //     offloadImageTopic.allocate<host_image_type>(
                //         host_image_type{image, IMG_WIDTH, IMG_HEIGHT}
                //     )
                // );

                imagePacket.put(
                    imagePacket.allocate<image_packet_type>(
                        image_packet_type{image}
                    )
                );
                
                free(packet.payload);

                switchboard::ptr<const gaze_type> gaze_ptr = _m_gaze.get_ro_nullable();
                if (gaze_ptr)
                {
                    float buffer[4] = {
                        gaze_ptr->gaze0[0], gaze_ptr->gaze0[1],
                        gaze_ptr->gaze1[0], gaze_ptr->gaze1[1]
                    };

                    assert(sizeof(buffer) == 16);
                    packet.header.command = CS_RSP_GAZE;
                    packet.header.payload_size = sizeof(buffer);
                    packet.payload = (char*)buffer;

                    size_t size_sent = send(&packet, sizeof(header_t));
                    STATUS_CHECK(size_sent != sizeof(header_t), "DEBUG: failed to send header");

                    if (packet.header.payload_size != 0)
                    {
                        size_t index = 0;
                        while (index < packet.header.payload_size)
                        {
                            size_t chunk_size = MIN(1024ul, packet.header.payload_size - index);
                            const char* data_ptr = packet.payload + index;
                            ssize_t bytes_sent = send(data_ptr, chunk_size);
                            STATUS_CHECK(bytes_sent == -1, "DEBUG: failed to send everything");
                            index += bytes_sent;
                        }
                    }
                }

#ifndef NDEBUG
                printf("received image: %d\n", imageIndex++);
#endif
            }
            else if (packet.header.command == CS_GRANT_TOKEN)
            {
                printf("Token granted\n");
            }
            else if (packet.header.command == CS_DEFINE_STEP)
            {
                printf("Step Defined: %d\n", *((int*)packet.payload));
            }
        }
    }

private:
    const std::shared_ptr<switchboard> _m_sb;
    const std::shared_ptr<pose_prediction> pp;
    switchboard::writer<host_image_type> offloadImageTopic;
    switchboard::writer<image_packet_type> imagePacket;
    switchboard::reader<gaze_type>         _m_gaze;
    int imageIndex = 0;

    switchboard::writer<network_throughput_type> netThroughput;
    std::chrono::_V2::system_clock::time_point timeStart;
    unsigned int byteReceived = 0;

    unsigned char imageBuffer[IMG_WIDTH * IMG_HEIGHT * IMG_CHN];
};

PLUGIN_MAIN(compute_offload)
