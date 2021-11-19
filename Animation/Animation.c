/*
 * Copyright (c) 2001 Fabrice Bellard
 *
 * Permission is hereby granted, free of charge, to any person obtaining a copy
 * of this software and associated documentation files (the "Software"), to deal
 * in the Software without restriction, including without limitation the rights
 * to use, copy, modify, merge, publish, distribute, sublicense, and/or sell
 * copies of the Software, and to permit persons to whom the Software is
 * furnished to do so, subject to the following conditions:
 *
 * The above copyright notice and this permission notice shall be included in
 * all copies or substantial portions of the Software.
 *
 * THE SOFTWARE IS PROVIDED "AS IS", WITHOUT WARRANTY OF ANY KIND, EXPRESS OR
 * IMPLIED, INCLUDING BUT NOT LIMITED TO THE WARRANTIES OF MERCHANTABILITY,
 * FITNESS FOR A PARTICULAR PURPOSE AND NONINFRINGEMENT. IN NO EVENT SHALL
 * THE AUTHORS OR COPYRIGHT HOLDERS BE LIABLE FOR ANY CLAIM, DAMAGES OR OTHER
 * LIABILITY, WHETHER IN AN ACTION OF CONTRACT, TORT OR OTHERWISE, ARISING FROM,
 * OUT OF OR IN CONNECTION WITH THE SOFTWARE OR THE USE OR OTHER DEALINGS IN
 * THE SOFTWARE.
 */

 /**
  * @file
  * video encoding with libavcodec API example
  *
  * @example encode_video.c
  */

#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include <libavcodec/avcodec.h>

#include <libavutil/opt.h>
#include <libavutil/imgutils.h>
#include <math.h>
#include <process.h>
#include <windows.h>

#define M_3_PI 0.954929658551372014613
#define NUM_THREADS 12

struct Complex {
    double re;
    double im;
};

double complex_len(struct Complex c) {
    return sqrt(c.re * c.re + c.im * c.im);
}

double complex_arg(struct Complex c) {
    double theta = atan2(c.im, c.re);
    return theta < 0 ? theta + 2.0 * M_PI : theta;
}

struct Complex from_polar(double len, double arg) {
    struct Complex c = { len * cos(arg), len * sin(arg) };
    return c;
}

struct Complex complex_add(struct Complex a, struct Complex b) {
    struct Complex sum = { a.re + b.re, a.im + b.im };
    return sum;
}

struct Complex complex_sub(struct Complex a, struct Complex b) {
    struct Complex sum = { a.re - b.re, a.im - b.im };
    return sum;
}

struct Complex complex_mul(struct Complex a, struct Complex b) {
    struct Complex product = { a.re * b.re - a.im * b.im, a.re * b.im + a.im * b.re };
    return product;
}

struct Complex complex_div(struct Complex a, struct Complex b) {
    double m = b.re * b.re + b.im * b.im;
    struct Complex div = {(a.re * b.re + a.im * b.im)/m, (a.im * b.re - a.re * b.im)/m};
    return div;
}

struct Complex complex_pow(struct Complex a, double b) {
    return from_polar(pow(complex_len(a), b), complex_arg(a) * b);
}

struct Complex complex_exp(struct Complex c) {
    return from_polar(exp(c.re), c.im);
}

struct rgb {
    uint8_t r;
    uint8_t b;
    uint8_t g;
};

struct rgb color(struct Complex c) {
    double hs = (complex_arg(c) * M_3_PI);
    double x = 1 - fabs(fmod(hs, 2.0) - 1);
    struct rgb result;
    switch ((int)hs) {
    case 0:
        result.r = 255;
        result.g = 255 * x;
        result.b = 0;
        return result;
    case 1:
        result.r = 255 * x;
        result.g = 255;
        result.b = 0;
        return result;
    case 2:
        result.r = 0;
        result.g = 255;
        result.b = 255 * x;
        return result;
    case 3:
        result.r = 0;
        result.g = 255 * x;
        result.b = 255;
        return result;
    case 4:
        result.r = 255 * x;
        result.g = 0;
        result.b = 255;
        return result;
    case 5:
        result.r = 255;
        result.g = 0;
        result.b = 255 * x;
        return result;
    case 6:
        result.r = 255;
        result.g = 0;
        result.b = 0;
        return result;
    }
}


static void encode(AVCodecContext* enc_ctx, AVFrame* frame, AVPacket* pkt,
    FILE* outfile)
{
    int ret;

    /* send the frame to the encoder */
    if (frame)
        printf("Send frame %3"PRId64"\n", frame->pts);

    ret = avcodec_send_frame(enc_ctx, frame);
    if (ret < 0) {
        fprintf(stderr, "Error sending a frame for encoding\n");
        exit(1);
    }

    while (ret >= 0) {
        ret = avcodec_receive_packet(enc_ctx, pkt);
        if (ret == AVERROR(EAGAIN) || ret == AVERROR_EOF)
            return;
        else if (ret < 0) {
            fprintf(stderr, "Error during encoding\n");
            exit(1);
        }

        printf("Write packet %3"PRId64" (size=%5d)\n", pkt->pts, pkt->size);
        fwrite(pkt->data, 1, pkt->size, outfile);
        av_packet_unref(pkt);
    }
}

struct rgb get_color(int x, int y, int i) {
    struct rgb res = { 255, 0 ,i };
    return res;
}

struct RenderThreadParams {
    int id; // id of the thread
    int i; // frame number
    AVFrame* frame; // AVFrame for pixel date to be written to
};

void prepare_frame_thread(void* params) {
    struct RenderThreadParams* p = params;
    AVFrame* frame = p->frame;
    double scaling_factor = 1.0 / 130;
    double start_re = -1 * (long long)(frame->width) / 2.0;
    double start_im = -1 * (long long)(frame->height) / 2.0;
    //struct Complex factor = from_polar(1.0, 0.1 * i);
    struct Complex c1 = { -2, -1 };
    struct Complex c2 = { 2, 2 };
    struct Complex c3 = { 1, 0 };
    for (int y = p->id; y < frame->height; y += NUM_THREADS) {
        for (int x = 0; x < frame->width; x++) {
            struct Complex in = { (start_re + x) * scaling_factor, (start_im + frame->height - y) * scaling_factor };
            struct Complex z_2 = complex_mul(in, in);
            struct Complex out = complex_mul(complex_sub(z_2, c3), complex_pow(complex_add(in, c1), 2.0 * .005 * p->i));
            out = complex_div(out, complex_add(z_2, c2));
            struct rgb c = color(out);
            int start = frame->linesize[0] * y + x * 4;
            frame->data[0][start] = c.r;
            frame->data[0][start + 1] = c.g;
            frame->data[0][start + 2] = c.b;
        }
    }
}



void prepare_frame_multithreaded(int i, AVFrame* frame) {
    HANDLE threads[NUM_THREADS];
    struct RenderThreadParams* params = malloc(NUM_THREADS * sizeof(struct RenderThreadParams));
    if (!params) {
        exit(1);
    }
    for (int t = 0; t < NUM_THREADS; t++) {
        params[t] = (struct RenderThreadParams){ t, i, frame };
        threads[t] = _beginthread(prepare_frame_thread, 0, params + t);
    }
    WaitForMultipleObjects(NUM_THREADS, threads, TRUE, INFINITE);
    free(params);
}

void prepare_frame(int i, AVFrame* frame) {
    double scaling_factor = 1.0 / 130;
    double start_re = -1 * (long long)(frame->width) / 2.0;
    double start_im = -1 * (long long)(frame->height) / 2.0;
    //struct Complex factor = from_polar(1.0, 0.1 * i);
    struct Complex c1 = { -2, -1};
    struct Complex c2 = { 2, 2 };
    struct Complex c3 = { 1, 0 };
    for (int x = 0; x < frame->width; x++) {
        for (int y = 0; y < frame->height; y++) {
            struct Complex in = { (start_re + x) * scaling_factor, (start_im +frame->height - y) * scaling_factor };
            struct Complex z_2 = complex_mul(in, in);
            struct Complex out = complex_mul(complex_sub(z_2, c3), complex_pow(complex_add(in, c1), 2.0 * .005 * i));
            out = complex_div(out, complex_add(z_2, c2));
            struct rgb c = color(out);
            int start = frame->linesize[0] * y + x * 4;
            frame->data[0][start] = c.r;
            frame->data[0][start + 1] = c.g;
            frame->data[0][start + 2] = c.b;
        }
    }
}

int main(int argc, char** argv)
{
    const char* filename, * codec_name;
    const AVCodec* codec;
    AVCodecContext* c = NULL;
    int i, ret, x, y;
    FILE* f;
    AVFrame* frame;
    AVPacket* pkt;
    uint8_t endcode[] = { 0, 0, 1, 0xb7 };

    if (argc <= 1) {
        fprintf(stderr, "Usage: %s <output file>\n", argv[0]);
        exit(0);
    }
    filename = argv[1];
    codec_name = "h264_nvenc";

    /* find the mpeg1video encoder */
    codec = avcodec_find_encoder_by_name(codec_name);
    if (!codec) {
        fprintf(stderr, "Codec '%s' not found\n", codec_name);
        exit(1);
    }

    c = avcodec_alloc_context3(codec);
    if (!c) {
        fprintf(stderr, "Could not allocate video codec context\n");
        exit(1);
    }

    pkt = av_packet_alloc();
    if (!pkt)
        exit(1);

    /* put sample parameters */
    c->bit_rate = 24000000;
    /* resolution must be a multiple of two */
    c->width = 2560;
    c->height = 1440;
    /* frames per second */
    c->time_base = (AVRational){ 1, 60 };
    c->framerate = (AVRational){ 60, 1 };

    /* emit one intra frame every ten frames
     * check frame pict_type before passing frame
     * to encoder, if frame->pict_type is AV_PICTURE_TYPE_I
     * then gop_size is ignored and the output of encoder
     * will always be I frame irrespective to gop_size
     */
    c->gop_size = 30;
    //c->max_b_frames = 2;
    c->pix_fmt = AV_PIX_FMT_RGB0;

    if (codec->id == AV_CODEC_ID_H264)
        av_opt_set(c->priv_data, "preset", "slow", 0);

    /* open it */
    ret = avcodec_open2(c, codec, NULL);
    if (ret < 0) {
        fprintf(stderr, "Could not open codec: %s\n", av_err2str(ret));
        exit(1);
    }

    f = fopen(filename, "wb");
    if (!f) {
        fprintf(stderr, "Could not open %s\n", filename);
        exit(1);
    }

    frame = av_frame_alloc();
    if (!frame) {
        fprintf(stderr, "Could not allocate video frame\n");
        exit(1);
    }
    frame->format = c->pix_fmt;
    frame->width = c->width;
    frame->height = c->height;

    ret = av_frame_get_buffer(frame, 0);
    if (ret < 0) {
        fprintf(stderr, "Could not allocate the video frame data\n");
        exit(1);
    }

    /* encode 1 second of video */
    for (i = 0; i < 3000; i++) {
        fflush(stdout);

        /* Make sure the frame data is writable.
           On the first round, the frame is fresh from av_frame_get_buffer()
           and therefore we know it is writable.
           But on the next rounds, encode() will have called
           avcodec_send_frame(), and the codec may have kept a reference to
           the frame in its internal structures, that makes the frame
           unwritable.
           av_frame_make_writable() checks that and allocates a new buffer
           for the frame only if necessary.
         */
        ret = av_frame_make_writable(frame);
        if (ret < 0) {
            exit(1);
        }
        prepare_frame_multithreaded(i, frame);

        frame->pts = i;

        /* encode the image */
        printf("Encoding frame %d\n", i);
        encode(c, frame, pkt, f);
    }

    /* flush the encoder */
    encode(c, NULL, pkt, f);

    /* Add sequence end code to have a real MPEG file.
       It makes only sense because this tiny examples writes packets
       directly. This is called "elementary stream" and only works for some
       codecs. To create a valid file, you usually need to write packets
       into a proper file format or protocol; see muxing.c.
     */
    if (codec->id == AV_CODEC_ID_MPEG1VIDEO || codec->id == AV_CODEC_ID_MPEG2VIDEO)
        fwrite(endcode, 1, sizeof(endcode), f);
    fclose(f);

    avcodec_free_context(&c);
    av_frame_free(&frame);
    av_packet_free(&pkt);

    return 0;
}