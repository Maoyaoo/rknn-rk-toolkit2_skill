// RKNN Matmul API skeleton (rknn_matmul_api.h).
//
// Distinct from the inference API: dedicated ctx type, dedicated set_io_mem,
// and shapes are described by rknn_matmul_info {M, K, N, type, layouts}.
//
//   rknn_matmul_create(&ctx, &info, &io_attr)
//   rknn_matmul_set_core_mask(ctx, mask)        // multi-core only; returns -1 elsewhere
//   rknn_matmul_set_quant_params(ctx, ...)      // INT8/hybrid-quant cases only
//   create A/B/C mem via rknn_create_mem(ctx, io_attr.X.size)
//   rknn_matmul_set_io_mem(ctx, A, &io_attr.A)  // x3
//   rknn_matmul_run(ctx)
//   rknn_matmul_destroy(ctx)
//
// B_layout / AC_layout:
//   0 = normal (M,K,N), 1 = native/perf layout (faster, dtype-specific tiling).
// Not all (matmul_type, layout, dtype) combos are valid for every SoC. Verify
// against the SDK release's matmul support table before relying on it.
//
// Source: distilled from the RKNN SDK V2.3.2 matmul API demo.
// Includes are .cpp because the matmul demo uses C++ helpers.
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/time.h>

#include "rknn_api.h"
#include "rknn_matmul_api.h"

static inline int64_t now_us() {
    struct timeval tv; gettimeofday(&tv, NULL);
    return (int64_t)tv.tv_sec * 1000000 + tv.tv_usec;
}

int main(int argc, char** argv) {
    if (argc < 4) {
        fprintf(stderr,
            "usage: %s <matmul_type> <M,K,N> <B_layout> [AC_layout] [loop_count] [core_mask]\n"
            "  matmul_type: 1=FP16xFP16->FP32, 2=INT8xINT8->INT32, 4=FP16xFP16->FP16,\n"
            "               7=FP16xINT4->FP32, 10=INT4xINT4->INT16 (see rknn_matmul_type enum)\n"
            "  B_layout/AC_layout: 0=normal, 1=native/perf\n",
            argv[0]);
        return 2;
    }

    int matmul_type = atoi(argv[1]);
    int M, K, N;
    if (sscanf(argv[2], "%d,%d,%d", &M, &K, &N) != 3) {
        fprintf(stderr, "M,K,N parse failed: %s\n", argv[2]); return 2;
    }
    int B_layout  = (argc > 3) ? atoi(argv[3]) : 0;
    int AC_layout = (argc > 4) ? atoi(argv[4]) : 0;
    int loop      = (argc > 5) ? atoi(argv[5]) : 10;
    int core_mask = (argc > 6) ? atoi(argv[6]) : 0;

    rknn_matmul_ctx ctx;
    rknn_matmul_info info;
    memset(&info, 0, sizeof(info));
    info.M          = M;
    info.K          = K;
    info.N          = N;
    info.type       = (rknn_matmul_type)matmul_type;
    info.B_layout   = B_layout;
    info.AC_layout  = AC_layout;
    info.iommu_domain_id = 0;

    rknn_matmul_io_attr io_attr;
    memset(&io_attr, 0, sizeof(io_attr));

    int ret = rknn_matmul_create(&ctx, &info, &io_attr);
    if (ret < 0) { fprintf(stderr, "rknn_matmul_create failed: %d\n", ret); return ret; }

    /* Optional, multi-core only; returns -1 on single-core platforms. */
    rknn_matmul_set_core_mask(ctx, (rknn_core_mask)core_mask);

    /* For INT8 / hybrid matmul, call rknn_matmul_set_quant_params for A/B/C here.
     * The official demo sets per-channel / per-group scale+zp depending on
     * matmul_type (RKNN_QUANT_TYPE_PER_CHANNEL_SYM, RKNN_QUANT_TYPE_PER_GROUP_SYM
     * with group_size=32, ...). Omitted in this skeleton for brevity. */

    rknn_tensor_mem* A = rknn_create_mem(ctx, io_attr.A.size);
    rknn_tensor_mem* B = rknn_create_mem(ctx, io_attr.B.size);
    rknn_tensor_mem* C = rknn_create_mem(ctx, io_attr.C.size);
    if (!A || !B || !C) { fprintf(stderr, "rknn_create_mem failed\n"); goto out; }

    /* Fill A/B with your real input. Layout depends on B_layout / AC_layout
     * (normal vs native/perf). For B normal→native conversion use the official
     * helper rather than rolling your own:
     *   rknn_B_normal_layout_to_native_layout(B_in, B_out, K, N,
     *                                        io_attr.B.dims[2], io_attr.B.dims[3],
     *                                        &info);
     * (RKNNRT API Ref §4.5; rknn_B_normal_layout_to_native_layout.)
     */
    memset((char*)A->virt_addr + A->offset, 0, A->size);
    memset((char*)B->virt_addr + B->offset, 0, B->size);

    if (rknn_matmul_set_io_mem(ctx, A, &io_attr.A) < 0 ||
        rknn_matmul_set_io_mem(ctx, B, &io_attr.B) < 0 ||
        rknn_matmul_set_io_mem(ctx, C, &io_attr.C) < 0) {
        fprintf(stderr, "rknn_matmul_set_io_mem failed\n"); goto out;
    }

    for (int i = 0; i < loop; ++i) {
        int64_t t0 = now_us();
        ret = rknn_matmul_run(ctx);
        int64_t dt = now_us() - t0;
        if (ret < 0) { fprintf(stderr, "rknn_matmul_run failed: %d\n", ret); goto out; }
        if (i == 0) printf("run[%d] = %.3f ms\n", i, dt / 1000.0);
    }

out:
    if (A) rknn_destroy_mem(ctx, A);
    if (B) rknn_destroy_mem(ctx, B);
    if (C) rknn_destroy_mem(ctx, C);
    rknn_matmul_destroy(ctx);
    return ret;
}
