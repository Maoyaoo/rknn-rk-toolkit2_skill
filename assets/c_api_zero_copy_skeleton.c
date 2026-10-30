/* RKNN Runtime zero-copy/external memory skeleton.
 * Use native attrs and size_with_stride. Adapt output layout before postprocess.
 */
#include <stdint.h>
#include <stdio.h>
#include <string.h>

#include "rknn_api.h"

int run_zero_copy(rknn_context ctx, const void* input_src, size_t input_size) {
    int ret;
    rknn_tensor_attr in_attr;
    rknn_tensor_attr out_attr;
    memset(&in_attr, 0, sizeof(in_attr));
    memset(&out_attr, 0, sizeof(out_attr));
    in_attr.index = 0;
    out_attr.index = 0;

    ret = rknn_query(ctx, RKNN_QUERY_NATIVE_INPUT_ATTR, &in_attr, sizeof(in_attr));
    if (ret != RKNN_SUCC) {
        printf("RKNN_QUERY_NATIVE_INPUT_ATTR failed: %d\n", ret);
        return ret;
    }
    ret = rknn_query(ctx, RKNN_QUERY_NATIVE_OUTPUT_ATTR, &out_attr, sizeof(out_attr));
    if (ret != RKNN_SUCC) {
        printf("RKNN_QUERY_NATIVE_OUTPUT_ATTR failed: %d\n", ret);
        return ret;
    }

    /* Many image demos use UINT8 input so normalize/quantize can be fused by RKNN. */
    in_attr.type = RKNN_TENSOR_UINT8;

    rknn_tensor_mem* in_mem = rknn_create_mem(ctx, in_attr.size_with_stride);
    rknn_tensor_mem* out_mem = rknn_create_mem(ctx, out_attr.size_with_stride);
    if (!in_mem || !out_mem) {
        printf("rknn_create_mem failed\n");
        if (in_mem) rknn_destroy_mem(ctx, in_mem);
        if (out_mem) rknn_destroy_mem(ctx, out_mem);
        return -1;
    }

    ret = rknn_set_io_mem(ctx, in_mem, &in_attr);
    if (ret != RKNN_SUCC) goto out;
    ret = rknn_set_io_mem(ctx, out_mem, &out_attr);
    if (ret != RKNN_SUCC) goto out;

    size_t copy_size = input_size < (size_t)in_mem->size ? input_size : (size_t)in_mem->size;
    memcpy(in_mem->virt_addr, input_src, copy_size);
    rknn_mem_sync(ctx, in_mem, RKNN_MEMORY_SYNC_TO_DEVICE);

    ret = rknn_run(ctx, NULL);
    if (ret != RKNN_SUCC) goto out;

    rknn_mem_sync(ctx, out_mem, RKNN_MEMORY_SYNC_FROM_DEVICE);
    printf("native output size_with_stride=%u virt=%p\n", out_attr.size_with_stride, out_mem->virt_addr);
    /* The output here is in the native layout (often NC1HWC2). Unpack to NCHW/NHWC
     * before model-specific postprocess, or query RKNN_QUERY_NATIVE_NHWC_OUTPUT_ATTR. */

out:
    rknn_destroy_mem(ctx, in_mem);
    rknn_destroy_mem(ctx, out_mem);
    return ret;
}
