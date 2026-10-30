// RKNN + RGA zero-copy pipeline skeleton.
//
// Pattern: load source image into a virtual-address buffer, wrap it as an RGA
// source; allocate the RKNN input mem with rknn_create_mem (which yields BOTH
// virt_addr AND a DMA fd), wrap that fd as the RGA destination, run imresize so
// RGA writes pixels straight into the RKNN input mem, then rknn_set_io_mem.
// No CPU copy of the resized tensor.
//
// Build prerequisites:
//   - librga: im2d.h, RgaUtils.h, rga.h, librga.so  (driver >= 1.2.4, librga >= 1.4)
//   - rknn_api.h + librknnrt.so (or librknnmrt.so on RV1103/1106)
//
// Source: distilled from the RKNN SDK V2.3.2 RKNN+RGA zero-copy demo.
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include "RgaUtils.h"
#include "im2d.h"
#include "rga.h"
#include "rknn_api.h"

#define STB_IMAGE_IMPLEMENTATION
#include "stb/stb_image.h"

#define ALIGN 8

int main(int argc, char** argv) {
    if (argc < 3) {
        fprintf(stderr, "usage: %s model.rknn input.jpg [loop_count]\n", argv[0]);
        return 2;
    }
    int loop_count = (argc > 3) ? atoi(argv[3]) : 1;

    /* 1. RKNN init + native input attr. */
    FILE* fp = fopen(argv[1], "rb");
    if (!fp) { fprintf(stderr, "open model failed\n"); return 1; }
    fseek(fp, 0, SEEK_END); long model_size = ftell(fp); fseek(fp, 0, SEEK_SET);
    unsigned char* model = (unsigned char*)malloc(model_size);
    if (fread(model, 1, model_size, fp) != (size_t)model_size) {
        fclose(fp); free(model); return 1;
    }
    fclose(fp);

    rknn_context ctx = 0;
    int ret = rknn_init(&ctx, model, model_size, 0, NULL);
    free(model);
    if (ret < 0) { fprintf(stderr, "rknn_init failed: %d\n", ret); return ret; }

    rknn_input_output_num io_num;
    memset(&io_num, 0, sizeof(io_num));
    rknn_query(ctx, RKNN_QUERY_IN_OUT_NUM, &io_num, sizeof(io_num));

    rknn_tensor_attr in_attr;
    memset(&in_attr, 0, sizeof(in_attr)); in_attr.index = 0;
    rknn_query(ctx, RKNN_QUERY_NATIVE_INPUT_ATTR, &in_attr, sizeof(in_attr));

    /* Native RKNN input is UINT8 NHWC for image demos. */
    in_attr.type = RKNN_TENSOR_UINT8;
    in_attr.fmt  = RKNN_TENSOR_NHWC;
    int H = in_attr.dims[1], W = in_attr.dims[2], C = in_attr.dims[3];
    int rga_fmt = (C == 3) ? RK_FORMAT_RGB_888 : RK_FORMAT_RGBA_8888;
    printf("native input HxWxC=%dx%dx%d size_with_stride=%u\n", H, W, C, in_attr.size_with_stride);

    /* 2. Allocate RKNN input mem (gives virt_addr + fd), then wrap its fd as
     *    an RGA destination. RGA will write resized pixels straight into it. */
    rknn_tensor_mem* in_mem = rknn_create_mem(ctx, in_attr.size_with_stride);
    if (!in_mem) { fprintf(stderr, "rknn_create_mem failed\n"); rknn_destroy(ctx); return 1; }

    /* Declared up front so the `goto out` error paths below do not cross
     * initializers (C++ forbids jumping past non-trivial initializers). */
    rknn_tensor_attr*  out_attrs = NULL;
    rknn_tensor_mem**  out_mems  = NULL;

    int wstride = W + (ALIGN - W % ALIGN) % ALIGN;
    int hstride = H;
    rga_buffer_t dst = wrapbuffer_fd_t(in_mem->fd, W, H, wstride, hstride, rga_fmt);

    /* 3. Load source image and wrap its virtual address as the RGA source. */
    int src_w = 0, src_h = 0, src_c = 0;
    unsigned char* src_data = stbi_load(argv[2], &src_w, &src_h, &src_c, C);
    if (!src_data) {
        fprintf(stderr, "stbi_load failed\n");
        rknn_destroy_mem(ctx, in_mem);
        rknn_destroy(ctx);
        return 1;
    }
    rga_buffer_t src = wrapbuffer_virtualaddr((void*)src_data, src_w, src_h, rga_fmt);

    im_rect src_rect = {0, 0, src_w, src_h};
    im_rect dst_rect = {0, 0, W, H};
    if (imcheck(src, dst, src_rect, dst_rect) != IM_STATUS_NOERROR) {
        fprintf(stderr, "imcheck rejected the conversion\n");
        STBI_FREE(src_data);
        rknn_destroy_mem(ctx, in_mem);
        rknn_destroy(ctx);
        return 1;
    }
    if (imresize(src, dst) != IM_STATUS_SUCCESS) {
        fprintf(stderr, "imresize failed\n");
        STBI_FREE(src_data);
        rknn_destroy_mem(ctx, in_mem);
        rknn_destroy(ctx);
        return 1;
    }
    STBI_FREE(src_data);

    /* 4. Hand the populated mem to RKNN as input (zero-copy). */
    ret = rknn_set_io_mem(ctx, in_mem, &in_attr);
    if (ret != RKNN_SUCC) {
        fprintf(stderr, "rknn_set_io_mem(in) failed: %d\n", ret);
        rknn_destroy_mem(ctx, in_mem);
        rknn_destroy(ctx);
        return ret;
    }

    /* 5. Allocate + set native output mems, then run. */
    out_attrs = (rknn_tensor_attr*)calloc(io_num.n_output, sizeof(*out_attrs));
    out_mems  = (rknn_tensor_mem**)calloc(io_num.n_output, sizeof(*out_mems));
    for (uint32_t i = 0; i < io_num.n_output; ++i) {
        out_attrs[i].index = i;
        rknn_query(ctx, RKNN_QUERY_NATIVE_OUTPUT_ATTR, &out_attrs[i], sizeof(out_attrs[i]));
        out_attrs[i].type = RKNN_TENSOR_FLOAT32;
        out_attrs[i].fmt  = RKNN_TENSOR_NCHW;
        out_mems[i] = rknn_create_mem(ctx, out_attrs[i].n_elems * sizeof(float));
        rknn_set_io_mem(ctx, out_mems[i], &out_attrs[i]);
    }

    for (int i = 0; i < loop_count; ++i) {
        ret = rknn_run(ctx, NULL);
        if (ret != RKNN_SUCC) { fprintf(stderr, "rknn_run failed: %d\n", ret); break; }
        for (uint32_t j = 0; j < io_num.n_output; ++j) {
            rknn_mem_sync(ctx, out_mems[j], RKNN_MEMORY_SYNC_FROM_DEVICE);
        }
        if (i == 0) {
            for (uint32_t j = 0; j < io_num.n_output; ++j) {
                printf("  output[%u] size=%u size_with_stride=%u virt=%p\n", j,
                       out_attrs[j].size, out_attrs[j].size_with_stride, out_mems[j]->virt_addr);
            }
        }
        /* Outputs are in the native layout (often NC1HWC2). Unpack to NCHW/NHWC
         * before model-specific postprocess, or query RKNN_QUERY_NATIVE_NHWC_OUTPUT_ATTR. */
    }

    for (uint32_t i = 0; i < io_num.n_output; ++i) {
        if (out_mems[i]) rknn_destroy_mem(ctx, out_mems[i]);
    }
    free(out_mems);
    free(out_attrs);

    rknn_destroy_mem(ctx, in_mem);
    rknn_destroy(ctx);
    return ret;
}
