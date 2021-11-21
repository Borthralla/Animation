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
#include <libavformat/avformat.h>
#include <libavutil/opt.h>
#include <libavutil/imgutils.h>
#include <math.h>
#include <process.h>
#include <windows.h>
#include <time.h>

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

struct Complex complex_sin(struct Complex c) {
    return (struct Complex){ sin(c.re)* cosh(c.im), cos(c.re)* sinh(c.im) };
}

struct Complex complex_cos(struct Complex c) {
    return (struct Complex){ cos(c.re)* cosh(c.im), -1 * sin(c.re)* sinh(c.im) };
}

struct Complex scalar_mul(struct Complex a, double b) {
    return (struct Complex) { a.re* b, a.im* b };
}

struct rgb {
    uint8_t r;
    uint8_t b;
    uint8_t g;
};

struct rgb color(struct Complex c) {
    double hs = (complex_arg(c) * M_3_PI);
    double x = 255 * (1 - fabs(fmod(hs, 2.0) - 1));
    struct rgb results[7];
    results[0] = (struct rgb){ 255 , x, 0};
    results[1] = (struct rgb){ x , 255, 0 };
    results[2] = (struct rgb){ 0 , 255, x };
    results[3] = (struct rgb){ 0 , x, 255 };
    results[4] = (struct rgb){ x , 0, 255 };
    results[5] = (struct rgb){ 255 , 0, x };
    results[6] = (struct rgb){ 255 , 0, 0 };
    return results[(int)hs];
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

void prepare_sin_frame_thread(void* params) {
    struct RenderThreadParams* p = params;
    AVFrame* frame = p->frame;
    double scaling_factor = 1.0 / 65;
    double start_re = -1 * (long long)(frame->width) / 2.0;
    double start_im = -1 * (long long)(frame->height) / 2.0;
    double i_q = p->i / 300.0;
    for (int y = p->id; y < frame->height; y += NUM_THREADS) {
        for (int x = 0; x < frame->width; x++) {
            struct Complex in = { (start_re + x) * scaling_factor, (start_im + frame->height - y) * scaling_factor };
            struct Complex out = complex_add(scalar_mul(complex_pow(in, 2), 1 - i_q), scalar_mul(complex_pow(in, 3), i_q));
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
        threads[t] = _beginthread(prepare_sin_frame_thread, 0, params + t);
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

int main(int argc, char** argv) {
    char* filename = argv[1];
    AVFormatContext* formatCtx = avformat_alloc_context();
    formatCtx->oformat = av_guess_format("mp4", NULL, NULL);
    AVIOContext* ioCtx;
    avio_open2(&ioCtx, filename, AVIO_FLAG_WRITE, NULL, NULL);
    formatCtx->pb = ioCtx;


    AVCodec* codec = avcodec_find_encoder_by_name("h264_nvenc");
    AVCodecContext* cdctx = avcodec_alloc_context3(codec);
    cdctx->width = 2560;
    cdctx->height = 1440;
    cdctx->time_base = (AVRational){ 1, 60 };
    cdctx->framerate = (AVRational){ 60, 1 };
    cdctx->bit_rate = 24000000;
    cdctx->gop_size = 30;
    cdctx->pix_fmt = AV_PIX_FMT_RGB0;

    AVStream* stream = avformat_new_stream(formatCtx, codec);
    avcodec_parameters_from_context(stream->codecpar, cdctx);
    stream->time_base = cdctx->time_base;

    av_dump_format(formatCtx, 0, filename, 1);
    avformat_write_header(formatCtx, NULL);

    double start = clock();

    // Prepare to start writing packets
    avcodec_open2(cdctx, codec, NULL);
    AVFrame* frame = av_frame_alloc();
    frame->format = cdctx->pix_fmt;
    frame->width = cdctx->width;
    frame->height = cdctx->height;
    av_frame_get_buffer(frame, 0);
    AVPacket* pkt = av_packet_alloc();
    int num_frames = 300;
    for (int i = 0; i < num_frames; i++) {
        av_frame_make_writable(frame);
        prepare_frame_multithreaded(i, frame);
        frame->pts = i;
        int send_frame_result = avcodec_send_frame(cdctx, frame);
        if (send_frame_result < 0) {
            fprintf(stderr, "Error sending a frame for encoding\n");
            exit(1);
        }
        int receive_packet_result = 1;
        while (receive_packet_result >= 0) {
            receive_packet_result = avcodec_receive_packet(cdctx, pkt);
            if (receive_packet_result == AVERROR(EAGAIN) || receive_packet_result == AVERROR_EOF)
                break;
            else if (receive_packet_result < 0) {
                fprintf(stderr, "Error during encoding\n");
                exit(1);
            }

            printf("Write packet %3"PRId64" (size=%5d)\n", pkt->pts, pkt->size);
            av_packet_rescale_ts(pkt, cdctx->time_base, stream->time_base);
            av_write_frame(formatCtx, pkt);
        }
    }
    avcodec_send_frame(cdctx, NULL);
    int receive_packet_result = avcodec_receive_packet(cdctx, pkt);
    while (receive_packet_result != AVERROR_EOF) {
        printf("Write packet %3"PRId64" (size=%5d)\n", pkt->pts, pkt->size);
        av_packet_rescale_ts(pkt, cdctx->time_base, stream->time_base);
        av_write_frame(formatCtx, pkt);   
        receive_packet_result = avcodec_receive_packet(cdctx, pkt);
    }
    printf("Done writing packets\n");
    av_write_trailer(formatCtx);
    printf("Done writing trailer\n");
    double end = clock();
    double duration = (end - start) / CLOCKS_PER_SEC;
    printf("Finished in %f seconds\n", duration);
    avformat_free_context(formatCtx);
    avcodec_free_context(&cdctx);
    av_frame_free(&frame);
    av_packet_free(&pkt);
    avio_close(ioCtx);
}