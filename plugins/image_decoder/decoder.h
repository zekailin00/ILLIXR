

extern "C"
{
#include <libavformat/avformat.h>
#include <libavcodec/avcodec.h>
#include <libswscale/swscale.h>
#include <libavutil/imgutils.h>
#include <libavutil/avutil.h>
#include <libavutil/pixdesc.h>
#include <libavcodec/avcodec.h>
#include <libavformat/avformat.h>
}

#include <functional>

class Decoder
{

public:
    using ReadStreamCb = int(*)(void *opaque, uint8_t *buf, int buf_size);

    Decoder(void* data, ReadStreamCb readStreamCb);
    ~Decoder();

    bool GetLatestFrame(unsigned char** outImage, int* outWidth, int* outHeight);

private:
    static void InitializeCodec(
        int* video_stream_index,
        AVFormatContext *fmt_ctx,
        AVCodecContext **codec_ctx,
        struct SwsContext **sws_ctx
    );

private:
    AVPacket *packet;
    AVFrame *frame;
    AVFrame *frame_rgb;

    int video_stream_index = -1;
    AVIOContext *avio_ctx;
    AVFormatContext *format_ctx = NULL;
    AVCodecContext *codec_ctx = NULL;
    struct SwsContext *sws_ctx = NULL;

    int frame_count = 0;
};
