/* RKNN Runtime external/internal memory reuse skeleton.
 *
 * Pattern: allocate ONE large internal-memory region sized for the largest of
 * N models, then have each context import it via rknn_create_mem_from_fd +
 * rknn_set_internal_mem. Lets multiple models (or multiple instances) share
 * the same DMA-able internal buffer instead of duplicating it N times.
 *
 * Init flags and per-context mem flags:
 *   rknn_init(..., RKNN_FLAG_MEM_ALLOC_OUTSIDE, NULL)
 *   rknn_query(ctx, RKNN_QUERY_MEM_SIZE, &mem_size, ...)
 *   shared = rknn_create_mem(ctx_max, max_internal_size)
 *   per_ctx = rknn_create_mem_from_fd(ctx, shared->fd, shared->virt_addr,
 *                                     mem_size_for_this_ctx, 0)
 *   rknn_set_internal_mem(ctx, per_ctx)
 *   optional: rknn_set_weight_mem(ctx, weight_mem)  // separate allocation
 *
 * Requires runtime with outside-alloc support. Check the SDK release notes for
 * the exact flag availability per SoC; this is NOT the RV110x zero-copy path
 * (see c_api_zero_copy_skeleton.c for that).
 *
 * Source: distilled from the RKNN SDK V2.3.2 internal memory reuse demo.
 */
#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include "rknn_api.h"

static unsigned char* load_file(const char* path, size_t* size) {
    FILE* fp = fopen(path, "rb");
    if (!fp) return NULL;
    fseek(fp, 0, SEEK_END);
    long len = ftell(fp);
    fseek(fp, 0, SEEK_SET);
    unsigned char* data = (unsigned char*)malloc((size_t)len);
    if (!data) { fclose(fp); return NULL; }
    if (fread(data, 1, (size_t)len, fp) != (size_t)len) {
        free(data); fclose(fp); return NULL;
    }
    fclose(fp);
    *size = (size_t)len;
    return data;
}

#define N_MODELS 2

int main(int argc, char** argv) {
    if (argc < 1 + N_MODELS) {
        fprintf(stderr, "usage: %s model_a.rknn [model_b.rknn ...] (need %d models)\n", argv[0], N_MODELS);
        return 2;
    }

    rknn_context ctx[N_MODELS]        = {0};
    rknn_mem_size mem_sz[N_MODELS];
    rknn_tensor_mem* shared_int = NULL;
    rknn_tensor_mem* int_mems[N_MODELS]   = {0};
    rknn_tensor_mem* weight_mems[N_MODELS] = {0};
    uint32_t max_internal = 0;
    int ret = 0;

    memset(mem_sz, 0, sizeof(mem_sz));

    /* 1. Init every model in outside-alloc mode and query its mem footprint. */
    for (int n = 0; n < N_MODELS; ++n) {
        size_t msize = 0;
        unsigned char* model = load_file(argv[1 + n], &msize);
        if (!model) { fprintf(stderr, "load %s failed\n", argv[1 + n]); ret = 1; goto out; }

        ret = rknn_init(&ctx[n], model, msize, RKNN_FLAG_MEM_ALLOC_OUTSIDE, NULL);
        free(model);
        if (ret < 0) { fprintf(stderr, "[%d] rknn_init failed: %d\n", n, ret); goto out; }

        ret = rknn_query(ctx[n], RKNN_QUERY_MEM_SIZE, &mem_sz[n], sizeof(mem_sz[n]));
        if (ret != RKNN_SUCC) {
            fprintf(stderr, "[%d] RKNN_QUERY_MEM_SIZE failed: %d\n", n, ret);
            goto out;
        }
        printf("[%d] weight=%u internal=%u\n", n,
               mem_sz[n].total_weight_size, mem_sz[n].total_internal_size);
        if (mem_sz[n].total_internal_size > max_internal) {
            max_internal = mem_sz[n].total_internal_size;
        }
    }

    /* 2. Allocate ONE shared internal region sized for the largest model. */
    printf("max_internal=%u — allocating shared region on ctx[0]\n", max_internal);
    shared_int = rknn_create_mem(ctx[0], max_internal);
    if (!shared_int) { fprintf(stderr, "rknn_create_mem(shared) failed\n"); ret = 1; goto out; }

    /* 3. Import the shared region into each context and (optionally) give each
     *    its own external weight memory. */
    for (int n = 0; n < N_MODELS; ++n) {
        int_mems[n] = rknn_create_mem_from_fd(
            ctx[n], shared_int->fd, shared_int->virt_addr,
            mem_sz[n].total_internal_size, 0);
        if (!int_mems[n]) { fprintf(stderr, "[%d] rknn_create_mem_from_fd failed\n", n); ret = 1; goto out; }

        ret = rknn_set_internal_mem(ctx[n], int_mems[n]);
        if (ret < 0) { fprintf(stderr, "[%d] rknn_set_internal_mem failed: %d\n", n, ret); goto out; }

        weight_mems[n] = rknn_create_mem(ctx[n], mem_sz[n].total_weight_size);
        if (weight_mems[n]) {
            rknn_set_weight_mem(ctx[n], weight_mems[n]);
        }
        printf("[%d] internal shared @fd=%d, weight mem allocated\n",
               n, int_mems[n]->fd);
    }

    /* 4. From here, set I/O mem, rknn_run, etc. just like the zero-copy
     *    skeleton. The internal/weight buffers are now reused across contexts. */
    printf("setup OK — proceed with rknn_set_io_mem + rknn_run per context\n");

out:
    if (shared_int) rknn_destroy_mem(ctx[0], shared_int);
    for (int n = 0; n < N_MODELS; ++n) {
        if (weight_mems[n]) rknn_destroy_mem(ctx[n], weight_mems[n]);
        /* int_mems[n] aliases the shared region above; do not double-free */
        if (ctx[n])         rknn_destroy(ctx[n]);
    }
    return ret;
}
