# Workflow: Board C/C++ runtime inference

## Read first

1. Always Read
2. `references/05_runtime_c_api.md`
3. Dynamic / external mem → also `references/11_dynamic_shape_and_mem_reuse.md`
4. Perf / zero-copy pipeline → `references/06_performance_memory.md`

## Steps

1. From SoC pick **API family**:
   - general: `rknn_init` → query → `rknn_inputs_set` → `rknn_run` → `rknn_outputs_get` (RK35xx / RV1126B class)
   - mini / zero-copy: `rknn_create_mem` + `rknn_set_io_mem` (RV110x / RK2118 — default; also preferred for high-throughput on full runtime)
2. Always query attrs (`SDK_VERSION`, IO num, INPUT/OUTPUT_ATTR; native attrs for zero-copy). Do not hard-code shapes from a single export if demos query at runtime.
3. Choose template:

   | Template | Use when |
   |---|---|
   | `assets/c_api_general_infer_template.c` | full-runtime board, single input, simplest baseline |
   | `assets/c_api_multi_input_template.c` | general API with 2+ inputs |
   | `assets/c_api_zero_copy_skeleton.c` | zero-copy `rknn_set_io_mem` baseline (RV110x / RK2118 / high-throughput) |
   | `assets/c_api_dynamic_shape_template.c` | `rknn_set_input_shapes` flow (not RV110x/RK2118) |
   | `assets/c_api_internal_mem_reuse_template.c` | `RKNN_FLAG_MEM_ALLOC_OUTSIDE` / shared internal mem |
   | `assets/c_api_rga_zero_copy_template.cpp` | camera/RGA → NPU zero-copy pipeline |
   | `assets/c_api_matmul_template.cpp` | standalone `rknn_matmul_*` API |
   | `assets/CMakeLists.rknn_template.txt` | CMake wiring; correct `.so` per arch |
4. Align width/layout rules (first-layer w_stride, NC1HWC2) when using zero-copy native layout.
5. Multi-core only where valid (RK3588/RK3576 core counts differ).
6. Smoke path before large app: `references/20_official_examples_and_smoke_paths.md`.

## Done when

- Generated code matches SoC API family and runtime library name.
- Query + destroy/release lifecycle present; CMake/lib path noted if build is in scope.
