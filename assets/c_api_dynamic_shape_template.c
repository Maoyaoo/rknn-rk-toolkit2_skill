/* RKNN Runtime dynamic-shape inference skeleton.
 *
 * Builds a model with `dynamic_input=[shape_set_A, shape_set_B, ...]` at
 * conversion time, then at runtime cycles through the supported shapes:
 *
 *   for each supported shape s in shape_range:
 *     rknn_set_input_shapes(ctx, n_input, input_attrs_with_s)
 *     rknn_query(ctx, RKNN_QUERY_CURRENT_INPUT_ATTR, ...)
 *     rknn_query(ctx, RKNN_QUERY_CURRENT_OUTPUT_ATTR, ...)
 *     (re)allocate input/output buffers for the current attrs
 *     rknn_inputs_set / rknn_run / rknn_outputs_get / rknn_outputs_release
 *
 * Platform gate: dynamic shape requires rknn_server + Runtime >= 1.5.0 and is
 * NOT supported on RV1103/RV1106/RV1103B/RV1106B/RK2118.
 *
 * Use for RK3562/RK3566/RK3568/RK3576/RK3588/RV1126B with the general API.
 * Source: distilled from the RKNN SDK V2.3.2 dynamic-shape runtime demo.
 */
#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include "rknn_api.h"

#define MAX_SHAPES 8

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

int main(int argc, char** argv) {
    if (argc < 3) {
        fprintf(stderr, "usage: %s model.rknn input.bin\n", argv[0]);
        return 2;
    }

    size_t model_size = 0;
    unsigned char* model = load_file(argv[1], &model_size);
    if (!model) { fprintf(stderr, "load model failed\n"); return 1; }

    rknn_context ctx = 0;
    int ret = rknn_init(&ctx, model, model_size, 0, NULL);
    free(model);
    if (ret < 0) { fprintf(stderr, "rknn_init failed: %d\n", ret); return ret; }

    rknn_input_output_num io_num;
    memset(&io_num, 0, sizeof(io_num));
    rknn_query(ctx, RKNN_QUERY_IN_OUT_NUM, &io_num, sizeof(io_num));
    printf("inputs=%u outputs=%u\n", io_num.n_input, io_num.n_output);

    /* 1. Query the dynamic input range the model was built with. */
    rknn_input_range range;
    memset(&range, 0, sizeof(range));
    range.index = 0;
    ret = rknn_query(ctx, RKNN_QUERY_INPUT_DYNAMIC_RANGE, &range, sizeof(range));
    if (ret != RKNN_SUCC) {
        fprintf(stderr, "RKNN_QUERY_INPUT_DYNAMIC_RANGE failed: %d\n"
                "Model was not built with dynamic_input, or target lacks support.\n", ret);
        rknn_destroy(ctx);
        return ret;
    }

    printf("supported shape_count=%u\n", range.shape_number);
    if (range.shape_number > MAX_SHAPES) {
        fprintf(stderr, "raise MAX_SHAPES (%u > %d)\n", range.shape_number, MAX_SHAPES);
        rknn_destroy(ctx);
        return 1;
    }

    /* 2. For each supported shape: set, re-query current attrs, run. */
    for (uint32_t s = 0; s < range.shape_number; ++s) {
        rknn_tensor_attr in_attr;
        memset(&in_attr, 0, sizeof(in_attr));
        in_attr.index = 0;
        for (uint32_t d = 0; d < range.n_dims && d < 16; ++d) {
            in_attr.dims[d] = range.dyn_range[s][d];
        }
        in_attr.n_dims = range.n_dims;

        ret = rknn_set_input_shapes(ctx, 1, &in_attr);
        if (ret != RKNN_SUCC) {
            fprintf(stderr, "[%u] rknn_set_input_shapes failed: %d\n", s, ret);
            continue;
        }

        /* Query the ACTUAL attrs the runtime will use for this shape. */
        rknn_tensor_attr cur_in, cur_out;
        memset(&cur_in, 0, sizeof(cur_in));  cur_in.index = 0;
        memset(&cur_out, 0, sizeof(cur_out)); cur_out.index = 0;
        rknn_query(ctx, RKNN_QUERY_CURRENT_INPUT_ATTR,  &cur_in,  sizeof(cur_in));
        rknn_query(ctx, RKNN_QUERY_CURRENT_OUTPUT_ATTR, &cur_out, sizeof(cur_out));
        printf("[%u] current input size=%u, output size=%u\n", s, cur_in.size, cur_out.size);

        /* (Re)allocate per current attrs. For simplicity we re-load a flat
         * binary input here; real apps resize/preprocess per shape. */
        size_t in_size = 0;
        unsigned char* input_data = load_file(argv[2], &in_size);
        if (!input_data) { fprintf(stderr, "load input failed\n"); continue; }

        rknn_input input;
        memset(&input, 0, sizeof(input));
        input.index = 0;
        input.buf = input_data;
        input.size = (uint32_t)in_size;
        input.type = cur_in.type;
        input.fmt  = cur_in.fmt;
        input.pass_through = 0;
        rknn_inputs_set(ctx, 1, &input);
        free(input_data);

        ret = rknn_run(ctx, NULL);
        if (ret != RKNN_SUCC) {
            fprintf(stderr, "[%u] rknn_run failed: %d\n", s, ret);
            continue;
        }

        rknn_output* outputs = (rknn_output*)calloc(io_num.n_output, sizeof(*outputs));
        for (uint32_t i = 0; i < io_num.n_output; ++i) {
            outputs[i].index = i;
            outputs[i].want_float = 0;
        }
        if (rknn_outputs_get(ctx, io_num.n_output, outputs, NULL) == RKNN_SUCC) {
            for (uint32_t i = 0; i < io_num.n_output; ++i) {
                printf("  output[%u] size=%u\n", i, outputs[i].size);
            }
            rknn_outputs_release(ctx, io_num.n_output, outputs);
        }
        free(outputs);
    }

    rknn_destroy(ctx);
    return 0;
}
