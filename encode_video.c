#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <libavformat/avformat.h>
#include <libavutil/log.h>
#include <libavutil/opt.h>
#include <libavutil/error.h>
#include <libavcodec/avcodec.h>
#include <libavutil/imgutils.h>
#include <errno.h>

static void encode(AVCodecContext* enc_ctx, AVFrame* frame, AVPacket* pkt, FILE* outfile)
{
    int ret;

    /* send the frame to the encoder */
    if (frame)
        printf("Send frame %3" PRId64 "\n", frame->pts);

    ret = avcodec_send_frame(enc_ctx, frame);
    if (ret < 0)
    {
        fprintf(stderr, "Error sending a frame for encoding\n");
        exit(1);
    }

    while (ret >= 0)
    {
        ret = avcodec_receive_packet(enc_ctx, pkt);
        if (ret == AVERROR(EAGAIN) || ret == AVERROR_EOF)
            return;
        else if (ret < 0)
        {
            fprintf(stderr, "Error during encoding\n");
            exit(1);
        }

        printf("Write packet %3" PRId64 " (size=%5d)\n", pkt->pts, pkt->size);
        fwrite(pkt->data, 1, pkt->size, outfile);
        av_packet_unref(pkt);
    }
}

int main(int argc, char* argv[])
{
    const char *    filename, *codec_name;
    const AVCodec*  codec;
    AVCodecContext* c = NULL;
    FILE*           f;
    AVFrame*        frame;
    AVPacket        pkt;
    uint8_t         endcode[] = {0, 0, 1, 0xb7};
    int             ret, i, x, y, got_output;
    if (argc <= 2)
    {
        fprintf(stderr, "Usage: %s <output file> <codec name>\n", argv[0]);
        exit(0);
    }

    filename   = argv[1];
    codec_name = argv[2];

    // 1.find the codec by name
    codec = avcodec_find_encoder_by_name(codec_name);
    if (!codec)
    {
        fprintf(stderr, "Codec not found\n");
        exit(1);
    }
    // 2.alloc codec context
    c = avcodec_alloc_context3(codec);
    if (!c)
    {
        fprintf(stderr, "Could not allocate video condec context\n");
        exit(1);
    }

    // 3.set encode parameters
    c->bit_rate  = 400000;
    c->width     = 720;
    c->height    = 480;
    c->time_base = (AVRational){1, 25};
    c->framerate = (AVRational){25, 1};

    c->gop_size     = 10;
    c->max_b_frames = 1;
    c->pix_fmt      = AV_PIX_FMT_YUV420P;

    if (codec->id == AV_CODEC_ID_H264)
        av_opt_set(c->priv_data, "preset", "slow", 0);

    // 4. open codec
    if (avcodec_open2(c, codec, NULL) < 0)
    {
        fprintf(stderr, "Could not open codec\n");
        exit(1);
    }

    f = fopen(filename, "w+");
    if (!f)
    {
        fprintf(stderr, "Could not open %s\n", filename);
        exit(1);
    }
    // 5.alloc frame store raw data
    frame = av_frame_alloc();
    if (!frame)
    {
        fprintf(stderr, "Could not alloc frame\n");
        exit(1);
    }
    // 6.set frame format
    frame->format = c->pix_fmt;
    frame->width  = c->width;
    frame->height = c->height;

    av_init_packet(&pkt);
    pkt.data = NULL; // packet data will be allocated by the encoder
    pkt.size = 0;

    ret = av_frame_get_buffer(frame, 32);
    if (ret < 0)
    {
        fprintf(stderr, "Could not allocate the video frame data\n");
        exit(1);
    }
    for (i = 0; i < 25; i++)
    {

        fflush(stdout);

        /* make sure the frame data is writable */
        ret = av_frame_make_writable(frame);
        if (ret < 0)
            exit(1);

        /* prepare a dummy image */
        /* Y */
        for (y = 0; y < c->height; y++)
        {
            for (x = 0; x < c->width; x++)
            {
                frame->data[0][y * frame->linesize[0] + x] = x + y + i * 3;
            }
        }

        /* Cb and Cr */
        for (y = 0; y < c->height / 2; y++)
        {
            for (x = 0; x < c->width / 2; x++)
            {
                frame->data[1][y * frame->linesize[1] + x] = 128 + y + i * 2;
                frame->data[2][y * frame->linesize[2] + x] = 64 + x + i * 5;
            }
        }

        frame->pts = i;

        // 7.encode the image
        /*        ret = avcodec_encode_video2(c, &pkt, frame, &got_output);
                if (ret < 0)
                {
                    fprintf(stderr, "Error encoding frame\n");
                    exit(1);
                }
                if (got_output)
                {
                    fwrite(pkt.data, 1, pkt.size, f);
                    av_packet_unref(&pkt);
                }
            }
        */
        /* encode the image */
        encode(c, frame, &pkt, f);
    }
    /* flush */
    encode(c, NULL, &pkt, f);
    fwrite(endcode, 1, sizeof(endcode), f);
    fclose(f);

    avcodec_free_context(&c);
    av_frame_free(&frame);
    av_packet_unref(&pkt);
    return 0;
}
