# Suggested AGENTS.md snippet

When working on Rockchip RKNN/RKNPU2 code, invoke `$rknn-rk-toolkit2` and follow its platform matrix. Always set `target_platform` explicitly, verify Toolkit2/rknn_server/runtime versions, check SoC-specific C API/operator support, and preserve Model Zoo preprocess/postprocess contracts before generating deployment code.
