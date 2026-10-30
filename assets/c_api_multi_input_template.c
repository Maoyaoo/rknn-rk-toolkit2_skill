/* RKNN Runtime multi-input general-API skeleton.
 *
 * Like c_api_general_infer_template.c but iterates over n_input instead of
 * hard-coding index 0. Use for models with 2+ inputs (e.g. image+template,
 * stereo, audio+visual). On multi-input builds, dataset.txt lines use one
 * file per input separated by spaces (see scripts/make_dataset_txt.py --multi-input).
 *
 * Use for RK3562/RK3566/RK3568/RK3576/RK3588/RV1126B. Do NOT use the
 * rknn_inputs_set / rknn_outputs_get path on RV1103/RV1106/RV1103B/RV1106B/RK2118.
 *
 * Source: distilled from the RKNN SDK V2.3.2 multi-input runtime demo.
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

/* Split argv input list on '#': "a.bin#b.bin" -> [a.bin, b.bin]. */
static int split_paths(char* s, char** out, int max) {
    int n = 0;
    char* save = NULL;
    for (char* p = strtok_r(s, "#", &save); p && n < max; p = strtok_r(NULL, "#", &save)) {
        out[n++] = p;
    }
    return n;
}

int main(int argc, char** argv) {
    if (argc < 3) {
        fprintf(stderr, "usage: %s model.rknn 'in0.bin#in1.bin[#in2.bin...]'\n", argv[0]);
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

    char* input_paths[16] = {0};
    int n_provided = split_paths(argv[2], input_paths, 16);
    if ((uint32_t)n_provided != io_num.n_input) {
        fprintf(stderr, "model needs %u inputs, got %d\n", io_num.n_input, n_provided);
        rknn_destroy(ctx);
        return 1;
    }

    rknn_tensor_attr* in_attrs = (rknn_tensor_attr*)calloc(io_num.n_input, sizeof(*in_attrs));
    for (uint32_t i = 0; i < io_num.n_input; ++i) {
        in_attrs[i].index = i;
        rknn_query(ctx, RKNN_QUERY_INPUT_ATTR, &in_attrs[i], sizeof(in_attrs[i]));
    }

    rknn_input* inputs = (rknn_input*)calloc(io_num.n_input, sizeof(*inputs));
    for (uint32_t i = 0; i < io_num.n_input; ++i) {
        size_t sz = 0;
        unsigned char* data = load_file(input_paths[i], &sz);
        if (!data) {
            fprintf(stderr, "load input[%u] %s failed\n", i, input_paths[i]);
            ret = 1; goto out;
        }
        inputs[i].index = i;
        inputs[i].buf = data;
        inputs[i].size = (uint32_t)sz;
        inputs[i].type = in_attrs[i].type;
        inputs[i].fmt  = in_attrs[i].fmt;
        inputs[i].pass_through = 0;
        /* data freed after rknn_inputs_set below */
        printf("input[%u] %s size=%u\n", i, input_paths[i], inputs[i].size);
    }

    ret = rknn_inputs_set(ctx, io_num.n_input, inputs);
    for (uint32_t i = 0; i < io_num.n_input; ++i) free(inputs[i].buf);
    if (ret != RKNN_SUCC) {
        fprintf(stderr, "rknn_inputs_set failed: %d\n", ret);
        goto out;
    }

    ret = rknn_run(ctx, NULL);
    if (ret != RKNN_SUCC) {
        fprintf(stderr, "rknn_run failed: %d\n", ret);
        goto out;
    }

    rknn_output* outputs = (rknn_output*)calloc(io_num.n_output, sizeof(*outputs));
    for (uint32_t i = 0; i < io_num.n_output; ++i) {
        outputs[i].index = i;
        outputs[i].want_float = 0;
        outputs[i].is_prealloc = 0;
    }
    if (rknn_outputs_get(ctx, io_num.n_output, outputs, NULL) == RKNN_SUCC) {
        for (uint32_t i = 0; i < io_num.n_output; ++i) {
            printf("output[%u] size=%u buf=%p\n", i, outputs[i].size, outputs[i].buf);
        }
        rknn_outputs_release(ctx, io_num.n_output, outputs);
    }
    free(outputs);

out:
    free(in_attrs);
    free(inputs);
    rknn_destroy(ctx);
    return ret;
}
