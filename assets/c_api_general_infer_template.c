/* RKNN Runtime general API inference skeleton.
 * Use for RK3562/RK3566/RK3568/RK3576/RK3588/RV1126B.
 * Do not use rknn_inputs_set/rknn_outputs_get on RV1103/RV1106/RK2118.
 */
#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include "rknn_api.h"

static unsigned char* load_file(const char* path, int* size) {
    FILE* fp = fopen(path, "rb");
    if (!fp) return NULL;
    fseek(fp, 0, SEEK_END);
    long len = ftell(fp);
    fseek(fp, 0, SEEK_SET);
    unsigned char* data = (unsigned char*)malloc((size_t)len);
    if (!data) {
        fclose(fp);
        return NULL;
    }
    if (fread(data, 1, (size_t)len, fp) != (size_t)len) {
        free(data);
        fclose(fp);
        return NULL;
    }
    fclose(fp);
    *size = (int)len;
    return data;
}

static void dump_tensor_attr(const rknn_tensor_attr* attr) {
    printf("index=%u name=%s n_dims=%u dims=[", attr->index, attr->name, attr->n_dims);
    for (uint32_t i = 0; i < attr->n_dims; ++i) {
        printf("%d%s", attr->dims[i], (i + 1 == attr->n_dims) ? "" : ",");
    }
    printf("] n_elems=%u size=%u fmt=%s type=%s qnt=%s zp=%d scale=%f\n",
           attr->n_elems, attr->size, get_format_string(attr->fmt),
           get_type_string(attr->type), get_qnt_type_string(attr->qnt_type),
           attr->zp, attr->scale);
}

int main(int argc, char** argv) {
    if (argc < 3) {
        fprintf(stderr, "usage: %s model.rknn input.bin\n", argv[0]);
        return 2;
    }

    int model_size = 0;
    int input_size = 0;
    unsigned char* model = load_file(argv[1], &model_size);
    unsigned char* input_data = load_file(argv[2], &input_size);
    if (!model || !input_data) {
        fprintf(stderr, "failed to load model or input\n");
        free(model);
        free(input_data);
        return 1;
    }

    rknn_context ctx = 0;
    int ret = rknn_init(&ctx, model, model_size, 0, NULL);
    free(model);
    if (ret < 0) {
        fprintf(stderr, "rknn_init failed: %d\n", ret);
        free(input_data);
        return ret;
    }

    rknn_sdk_version sdk_ver;
    memset(&sdk_ver, 0, sizeof(sdk_ver));
    if (rknn_query(ctx, RKNN_QUERY_SDK_VERSION, &sdk_ver, sizeof(sdk_ver)) == RKNN_SUCC) {
        printf("rknn api/runtime: %s driver: %s\n", sdk_ver.api_version, sdk_ver.drv_version);
    }

    rknn_input_output_num io_num;
    memset(&io_num, 0, sizeof(io_num));
    ret = rknn_query(ctx, RKNN_QUERY_IN_OUT_NUM, &io_num, sizeof(io_num));
    if (ret != RKNN_SUCC || io_num.n_input < 1) {
        fprintf(stderr, "RKNN_QUERY_IN_OUT_NUM failed: %d\n", ret);
        rknn_destroy(ctx);
        free(input_data);
        return ret;
    }
    printf("inputs=%u outputs=%u\n", io_num.n_input, io_num.n_output);

    rknn_tensor_attr input_attr;
    memset(&input_attr, 0, sizeof(input_attr));
    input_attr.index = 0;
    ret = rknn_query(ctx, RKNN_QUERY_INPUT_ATTR, &input_attr, sizeof(input_attr));
    if (ret != RKNN_SUCC) {
        fprintf(stderr, "RKNN_QUERY_INPUT_ATTR failed: %d\n", ret);
        rknn_destroy(ctx);
        free(input_data);
        return ret;
    }
    dump_tensor_attr(&input_attr);

    for (uint32_t i = 0; i < io_num.n_output; ++i) {
        rknn_tensor_attr out_attr;
        memset(&out_attr, 0, sizeof(out_attr));
        out_attr.index = i;
        if (rknn_query(ctx, RKNN_QUERY_OUTPUT_ATTR, &out_attr, sizeof(out_attr)) == RKNN_SUCC) {
            dump_tensor_attr(&out_attr);
        }
    }

    rknn_custom_string custom_string;
    memset(&custom_string, 0, sizeof(custom_string));
    if (rknn_query(ctx, RKNN_QUERY_CUSTOM_STRING, &custom_string, sizeof(custom_string)) == RKNN_SUCC) {
        printf("custom string: %s\n", custom_string.string);
    }

    if ((uint32_t)input_size != input_attr.size) {
        printf("warning: input file size=%d, model input size=%u\n", input_size, input_attr.size);
    }

    rknn_input input;
    memset(&input, 0, sizeof(input));
    input.index = 0;
    input.buf = input_data;
    input.size = (uint32_t)input_size;
    input.type = input_attr.type;
    input.fmt = input_attr.fmt;
    input.pass_through = 0;
    ret = rknn_inputs_set(ctx, 1, &input);
    free(input_data);
    if (ret != RKNN_SUCC) {
        fprintf(stderr, "rknn_inputs_set failed: %d\n", ret);
        rknn_destroy(ctx);
        return ret;
    }

    ret = rknn_run(ctx, NULL);
    if (ret != RKNN_SUCC) {
        fprintf(stderr, "rknn_run failed: %d\n", ret);
        rknn_destroy(ctx);
        return ret;
    }

    rknn_output* outputs = (rknn_output*)calloc(io_num.n_output, sizeof(*outputs));
    for (uint32_t i = 0; i < io_num.n_output; ++i) {
        outputs[i].index = i;
        outputs[i].want_float = 0;
    }
    ret = rknn_outputs_get(ctx, io_num.n_output, outputs, NULL);
    if (ret == RKNN_SUCC) {
        for (uint32_t i = 0; i < io_num.n_output; ++i) {
            printf("output[%u] size=%u buf=%p\n", i, outputs[i].size, outputs[i].buf);
        }
        rknn_outputs_release(ctx, io_num.n_output, outputs);
    }
    free(outputs);
    rknn_destroy(ctx);
    return ret;
}
