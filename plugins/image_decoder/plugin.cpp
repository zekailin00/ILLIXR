#include "illixr/data_format.hpp"
#include "illixr/managed_thread.hpp"
#include "illixr/phonebook.hpp"
#include "illixr/relative_clock.hpp"
#include "illixr/switchboard.hpp"
#include "illixr/threadloop.hpp"

#include "decoder.h"

#include <stdbool.h>
#include <unistd.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <deque>
#include <mutex>


using namespace ILLIXR;

class image_decoder: public ILLIXR::threadloop
{
public:
    image_decoder(const std::string& name_, phonebook* pb_):
        threadloop{name_, pb_},
        _m_sb{pb->lookup_impl<switchboard>()},
        offloadImageTopic{_m_sb->get_writer<host_image_type>("host_image")}
        {
            _m_sb->schedule<image_packet_type>(
                id, "image_packet",
                [&](switchboard::ptr<const image_packet_type>&& event3, std::size_t size)
                {
                    // printf("get packet with size: %d\n", size);
                    int packetIndex = 0;
                    while(packetIndex < event3->packet.size()) {
                        std::lock_guard<std::mutex> guard(this->mtx);
                        this->framePackets.push_back(*(event3->packet.data() + packetIndex));
                        packetIndex++;
                    }
                }
            );
        }
    
    std::deque<unsigned char> framePackets{};

protected:
    static int _read_packets(void *opaque, uint8_t *buf, int buf_size)
    {
        image_decoder* _this = (image_decoder*) opaque;
        std::lock_guard<std::mutex> guard(_this->mtx);

        int dataRead = 0;
        while (dataRead < buf_size && !_this->framePackets.empty())
        {
            *(buf + dataRead) = _this->framePackets.front();
            _this->framePackets.pop_front();
            dataRead++;
        }

        // printf("[_read_packets] data read: %d, data requested: %d\n", dataRead, buf_size);
        return dataRead;
    }

    void _p_one_iteration() override
    {
        if (decoder == nullptr)
            decoder = new Decoder(this, image_decoder::_read_packets);

        unsigned char *outImage;
        int outWidth, outHeight;

        // printf("DEBUG: get latest frame.\n");
        bool result = decoder->GetLatestFrame(&outImage, &outWidth, &outHeight);
        if (!result)
            return;
    
        int payloadSize = outWidth * outHeight * 4;
        std::vector<uint8_t> image;
        image.resize(payloadSize);
        memcpy(image.data(), outImage, payloadSize);

        printf("width: %d, height: %d, payloadSize: %d\n", outWidth, outHeight, payloadSize);

        offloadImageTopic.put(
            offloadImageTopic.allocate<host_image_type>(
                host_image_type{image, outWidth, outHeight}
            )
        );
    }

private:
    const std::shared_ptr<switchboard> _m_sb;
    switchboard::writer<host_image_type> offloadImageTopic;
    int imageIndex = 0;

    std::mutex mtx;
    Decoder* decoder = nullptr;
};

PLUGIN_MAIN(image_decoder)
