#include "decoder.h"


void Decoder::InitializeCodec(int* video_stream_index,
    AVFormatContext *fmt_ctx, AVCodecContext **codec_ctx, struct SwsContext **sws_ctx)
{
    *video_stream_index = -1;
    for (int i = 0; i < fmt_ctx->nb_streams; i++) {
        if (fmt_ctx->streams[i]->codecpar->codec_type == AVMEDIA_TYPE_VIDEO) {
            *video_stream_index = i;
            break;
        }
    }
    if (*video_stream_index == -1) {
        fprintf(stderr, "Could not find video stream.\n");
        avformat_close_input(&fmt_ctx);
        throw "Could not find video stream.";
    }

    AVCodecParameters *codec_params = fmt_ctx->streams[*video_stream_index]->codecpar;
    const AVCodec *codec = (const AVCodec*)avcodec_find_decoder(codec_params->codec_id);
    if (!codec) {
        fprintf(stderr, "Unsupported codec.\n");
        avformat_close_input(&fmt_ctx);
        throw "Unsupported codec.";
    }

    *codec_ctx = avcodec_alloc_context3(codec);
    avcodec_parameters_to_context(*codec_ctx, codec_params);
    if (avcodec_open2(*codec_ctx, codec, NULL) < 0) {
        fprintf(stderr, "Could not open codec.\n");
        avcodec_free_context(codec_ctx);
        avformat_close_input(&fmt_ctx);
        throw "Could not open codec.";
    }

    *sws_ctx = sws_getContext(
        (*codec_ctx)->width, (*codec_ctx)->height, (*codec_ctx)->pix_fmt,
        (*codec_ctx)->width, (*codec_ctx)->height, AV_PIX_FMT_RGBA,
        SWS_BILINEAR, NULL, NULL, NULL
    );

    return;
}

Decoder::Decoder(void* data, ReadStreamCb readStreamCb)
{
    size_t avio_ctx_buffer_size = 4096;
    uint8_t *avio_ctx_buffer = (uint8_t*) av_malloc(avio_ctx_buffer_size);
    if (!avio_ctx_buffer) {
        throw AVERROR(ENOMEM);
    }

    AVIOContext *avio_ctx = avio_alloc_context(
        avio_ctx_buffer, avio_ctx_buffer_size, 0,
        data, readStreamCb, NULL, NULL
    );
    if (!avio_ctx) {
        throw AVERROR(ENOMEM);
    }

    if (!(format_ctx = avformat_alloc_context())) {
        throw AVERROR(ENOMEM);
    }
    format_ctx->pb = avio_ctx;

    AVDictionary* options = nullptr;
    av_dict_set(&options, "probesize", "5000000", 0); // 5 MB
    av_dict_set(&options, "analyzeduration", "50000000", 0); // 5 seconds


    if (avformat_open_input(&format_ctx, NULL, NULL, &options) != 0)
    {
        fprintf(stderr, "Could not open input file.\n");
        throw "Could not open input file.";
    }

    if (avformat_find_stream_info(format_ctx, NULL) < 0)
    {
        fprintf(stderr, "Could not find stream info.\n");
        avformat_close_input(&format_ctx);
        throw "Could not find stream info.";
    }

    InitializeCodec(&video_stream_index, format_ctx, &codec_ctx, &sws_ctx);
    
    packet = av_packet_alloc();
    frame = av_frame_alloc();
    frame_rgb = av_frame_alloc();
    frame_rgb->format = AV_PIX_FMT_RGBA;
    frame_rgb->width  = codec_ctx->width;
    frame_rgb->height = codec_ctx->height;
    int ret = av_frame_get_buffer(frame_rgb, 0);
    if (ret < 0) {
        fprintf(stderr, "Could not allocate the video frame data\n");
        throw "Could not allocate the video frame data";
    }    
}


Decoder::~Decoder()
{
    sws_freeContext(sws_ctx);
    av_frame_free(&frame);
    av_frame_free(&frame_rgb);
    av_packet_free(&packet);
    avcodec_free_context(&codec_ctx);
    avformat_close_input(&format_ctx);

    if (avio_ctx)
        av_freep(&avio_ctx->buffer);
    avio_context_free(&avio_ctx);
}

bool Decoder::GetLatestFrame(unsigned char** outImage, int* outWidth, int* outHeight)
{
    if (av_read_frame(format_ctx, packet) >= 0)
    {
        if (packet->stream_index == video_stream_index &&
            avcodec_send_packet(codec_ctx, packet) == 0 &&
            avcodec_receive_frame(codec_ctx, frame) == 0)
        {
            int ret = sws_scale(sws_ctx,
                (uint8_t const * const *)frame->data, frame->linesize,
                0, codec_ctx->height,
                frame_rgb->data, frame_rgb->linesize
            );

            if (ret < 0)
            {
                char error_buf[AV_ERROR_MAX_STRING_SIZE];
                av_strerror(ret, error_buf, AV_ERROR_MAX_STRING_SIZE);
                fprintf(stderr, "Error receiving packet: %s\n", error_buf);
            }

            if (frame_rgb->data != nullptr)
            {
                *outImage  = frame_rgb->data[0];
                *outHeight = frame->height;
                *outWidth  = frame->width;
                // printf("Compression rate: %f percent\n", packet->size * 1.0f / (frame->width * frame->height * 3.0f) * 100);
                return true;
            }

            frame_count++;
        }
        av_packet_unref(packet);
    }

    return false;
}
