#!/usr/bin/env bash
# Cheap regression check for the rknn-rk-toolkit2 skill.
# - py_compile every Python script
# - run --help on CLI scripts (argparse exercises imports)
# - syntax-check every C/C++ template against minimal RKNN/RGA stubs
# - confirm every reference doc is non-empty
# This does NOT require RKNN, adb, librga, or a board.
set -u

HERE="$(cd "$(dirname "${BASH_SOURCE[0]}")" && pwd)"
ROOT="$(cd "$HERE/.." && pwd)"

status=0
TMPSTUBS="$(mktemp -d)"
trap 'rm -rf "$TMPSTUBS"' EXIT
# Keep py_compile bytecode out of the skill tree.
export PYTHONPYCACHEPREFIX="$TMPSTUBS/pycache"

build_stubs() {
  # Minimal stub headers so the C/C++ templates can be parsed without the SDK.
  cat > "$TMPSTUBS/rknn_api.h" <<'EOF'
#ifndef RKNN_API_H
#define RKNN_API_H
#include <stdint.h>
#include <stddef.h>
typedef struct _rknn_context* rknn_context;
typedef struct {uint32_t n_input,n_output;} rknn_input_output_num;
typedef struct {char name[256];uint32_t index,n_dims,dims[16],n_elems,size,w_stride,size_with_stride;int fmt,type,qnt_type;int zp;float scale;} rknn_tensor_attr;
typedef struct {char api_version[256];char drv_version[256];} rknn_sdk_version;
typedef struct {char string[1024];} rknn_custom_string;
typedef struct {void* virt_addr;unsigned long phys_addr;int fd;uint32_t size;uint32_t offset;} rknn_tensor_mem;
typedef struct {uint32_t total_weight_size,total_internal_size;size_t total_dma_allocated_size;} rknn_mem_size;
typedef struct {uint32_t index;char name[256];uint32_t n_dims,shape_number;int dyn_range[8][16];int fmt;} rknn_input_range;
typedef struct {uint32_t index;void* buf;uint32_t size;int type,fmt;uint8_t pass_through;} rknn_input;
typedef struct {uint32_t index;void* buf;uint32_t size;uint8_t want_float,is_prealloc;} rknn_output;
#define RKNN_SUCC 0
#define RKNN_FLAG_MEM_ALLOC_OUTSIDE 1
#define RKNN_QUERY_SDK_VERSION 1
#define RKNN_QUERY_IN_OUT_NUM 2
#define RKNN_QUERY_INPUT_ATTR 3
#define RKNN_QUERY_OUTPUT_ATTR 4
#define RKNN_QUERY_CUSTOM_STRING 5
#define RKNN_QUERY_NATIVE_INPUT_ATTR 6
#define RKNN_QUERY_NATIVE_OUTPUT_ATTR 7
#define RKNN_QUERY_MEM_SIZE 8
#define RKNN_QUERY_INPUT_DYNAMIC_RANGE 9
#define RKNN_QUERY_CURRENT_INPUT_ATTR 10
#define RKNN_QUERY_CURRENT_OUTPUT_ATTR 11
enum{RKNN_TENSOR_NCHW,RKNN_TENSOR_NHWC,RKNN_TENSOR_NC1HWC2,RKNN_TENSOR_UNDEFINED};
enum{RKNN_TENSOR_FLOAT32,RKNN_TENSOR_FLOAT16,RKNN_TENSOR_INT8,RKNN_TENSOR_UINT8,RKNN_TENSOR_INT16,RKNN_TENSOR_UINT16,RKNN_TENSOR_INT32,RKNN_TENSOR_UINT32,RKNN_TENSOR_INT64,RKNN_TENSOR_BOOL,RKNN_TENSOR_INT4,RKNN_TENSOR_BFLOAT16};
#define RKNN_NPU_CORE_0 1
#define RKNN_MEMORY_SYNC_TO_DEVICE 1
#define RKNN_MEMORY_SYNC_FROM_DEVICE 2
typedef unsigned long rknn_core_mask;
static inline int rknn_init(rknn_context*c,const void*m,size_t s,uint32_t f,void*e){(void)c;(void)m;(void)s;(void)f;(void)e;return 0;}
static inline int rknn_query(rknn_context c,int q,void*a,size_t l){(void)c;(void)q;(void)a;(void)l;return 0;}
static inline int rknn_inputs_set(rknn_context c,uint32_t n,const rknn_input*i){(void)c;(void)n;(void)i;return 0;}
static inline int rknn_run(rknn_context c,void*e){(void)c;(void)e;return 0;}
static inline int rknn_outputs_get(rknn_context c,uint32_t n,rknn_output*o,void*e){(void)c;(void)n;(void)o;(void)e;return 0;}
static inline int rknn_outputs_release(rknn_context c,uint32_t n,rknn_output*o){(void)c;(void)n;(void)o;return 0;}
static inline void rknn_destroy(rknn_context c){(void)c;}
static inline rknn_tensor_mem* rknn_create_mem(rknn_context c,uint32_t s){(void)c;(void)s;static rknn_tensor_mem m;return &m;}
static inline rknn_tensor_mem* rknn_create_mem_from_fd(rknn_context c,int fd,void*v,uint32_t s,uint32_t f){(void)c;(void)fd;(void)v;(void)s;(void)f;static rknn_tensor_mem m2;return &m2;}
static inline int rknn_set_io_mem(rknn_context c,rknn_tensor_mem*m,rknn_tensor_attr*a){(void)c;(void)m;(void)a;return 0;}
static inline int rknn_set_internal_mem(rknn_context c,rknn_tensor_mem*m){(void)c;(void)m;return 0;}
static inline int rknn_set_weight_mem(rknn_context c,rknn_tensor_mem*m){(void)c;(void)m;return 0;}
static inline int rknn_set_input_shapes(rknn_context c,uint32_t n,rknn_tensor_attr*a){(void)c;(void)n;(void)a;return 0;}
static inline int rknn_set_core_mask(rknn_context c,rknn_core_mask m){(void)c;(void)m;return 0;}
static inline int rknn_mem_sync(rknn_context c,rknn_tensor_mem*m,int d){(void)c;(void)m;(void)d;return 0;}
static inline void rknn_destroy_mem(rknn_context c,rknn_tensor_mem*m){(void)c;(void)m;}
static inline const char* get_format_string(int x){(void)x;return "?";}
static inline const char* get_type_string(int x){(void)x;return "?";}
static inline const char* get_qnt_type_string(int x){(void)x;return "?";}
#endif
EOF
  cat > "$TMPSTUBS/rknn_matmul_api.h" <<'EOF'
#ifndef RKNN_MATMUL_API_H
#define RKNN_MATMUL_API_H
#include "rknn_api.h"
typedef rknn_context rknn_matmul_ctx;
typedef int rknn_matmul_type;
typedef struct {int M,K,N,type,B_layout,AC_layout,iommu_domain_id,B_quant_type,group_size;} rknn_matmul_info;
typedef struct {rknn_tensor_attr A,B,C;} rknn_matmul_io_attr;
static inline int rknn_matmul_create(rknn_matmul_ctx*c,rknn_matmul_info*i,rknn_matmul_io_attr*a){(void)c;(void)i;(void)a;return 0;}
static inline int rknn_matmul_set_io_mem(rknn_matmul_ctx c,rknn_tensor_mem*m,void*a){(void)c;(void)m;(void)a;return 0;}
static inline int rknn_matmul_run(rknn_matmul_ctx c){(void)c;return 0;}
static inline int rknn_matmul_set_core_mask(rknn_matmul_ctx c,rknn_core_mask m){(void)c;(void)m;return 0;}
static inline void rknn_matmul_destroy(rknn_matmul_ctx c){(void)c;}
#endif
EOF
  cat > "$TMPSTUBS/RgaUtils.h" <<'EOF'
#ifndef RGAUTILS_H
#define RGAUTILS_H
#endif
EOF
  cat > "$TMPSTUBS/im2d.h" <<'EOF'
#ifndef IM2D_H
#define IM2D_H
#define RK_FORMAT_RGB_888 1
#define RK_FORMAT_RGBA_8888 2
#define IM_STATUS_NOERROR 0
#define IM_STATUS_SUCCESS 0
typedef int rga_buffer_handle_t;
typedef int IM_STATUS;
typedef struct {int x,y,width,height;} im_rect;
typedef struct {int dummy;} rga_buffer_t;
static inline rga_buffer_t wrapbuffer_virtualaddr(void*p,int w,int h,int f){rga_buffer_t b;(void)p;(void)w;(void)h;(void)f;return b;}
static inline rga_buffer_t wrapbuffer_fd_t(int fd,int w,int h,int ws,int hs,int f){rga_buffer_t b;(void)fd;(void)w;(void)h;(void)ws;(void)hs;(void)f;return b;}
static inline IM_STATUS imcheck(rga_buffer_t s,rga_buffer_t d,im_rect sr,im_rect dr){(void)s;(void)d;(void)sr;(void)dr;return 0;}
static inline IM_STATUS imresize(rga_buffer_t s,rga_buffer_t d){(void)s;(void)d;return 0;}
static inline const char* imStrError(IM_STATUS e){(void)e;return "?";}
#endif
EOF
  cat > "$TMPSTUBS/rga.h" <<'EOF'
#ifndef RGA_H
#define RGA_H
#endif
EOF
  mkdir -p "$TMPSTUBS/stb"
  cat > "$TMPSTUBS/stb/stb_image.h" <<'EOF'
#ifndef STB_IMAGE_H
#define STB_IMAGE_H
#include <stdlib.h>
#define STB_IMAGE_IMPLEMENTATION
#define STBI_MALLOC(sz) malloc(sz)
#define STBI_FREE(p) free(p)
static inline unsigned char* stbi_load(const char*f,int*x,int*y,int*c,int n){(void)f;(void)x;(void)y;(void)c;(void)n;return NULL;}
#endif
EOF
}

echo "== py_compile =="
for py in "$ROOT"/scripts/*.py "$ROOT"/assets/*.py; do
  [ -f "$py" ] || continue
  if python3 -m py_compile "$py"; then
    echo "ok: $py"
  else
    echo "FAIL: $py"
    status=1
  fi
done

echo
echo "== --help (scripts with argparse) =="
for py in "$ROOT"/scripts/make_dataset_txt.py "$ROOT"/scripts/rknn_env_probe.py "$ROOT"/scripts/rknn_server_commands.py \
         "$ROOT"/assets/convert_onnx_to_rknn.py "$ROOT"/assets/convert_pytorch_to_rknn.py "$ROOT"/assets/accuracy_analysis_template.py; do
  [ -f "$py" ] || continue
  # Templates that import rknn.api cannot run --help without the SDK installed.
  case "$py" in
    */assets/convert_*.py|*/assets/accuracy_analysis_template.py)
      echo "skip (imports rknn.api): $py"
      continue ;;
  esac
  if python3 "$py" --help >/dev/null 2>&1; then
    echo "ok: $py --help"
  else
    echo "FAIL: $py --help"
    status=1
  fi
done

echo
echo "== C/C++ template syntax check =="
if command -v gcc >/dev/null 2>&1; then
  build_stubs
  for tmpl in "$ROOT"/assets/*.c "$ROOT"/assets/*.cpp; do
    [ -f "$tmpl" ] || continue
    case "$tmpl" in
      *.c)  CC=gcc ; FLAGS="-std=gnu11" ;;
      *.cpp) CC=g++ ; FLAGS="-std=gnu++11" ;;
    esac
    if $CC $FLAGS -fsyntax-only -I"$TMPSTUBS" "$tmpl" >/dev/null 2>&1; then
      echo "ok: $tmpl"
    else
      echo "FAIL: $tmpl"
      $CC $FLAGS -fsyntax-only -I"$TMPSTUBS" "$tmpl" 2>&1 | head -6 | sed 's/^/    /'
      status=1
    fi
  done
else
  echo "skip (gcc/g++ not available)"
fi

echo
echo "== reference docs =="
shopt -s nullglob
for ref in "$ROOT"/references/*.md; do
  size=$(wc -c <"$ref")
  if [ "$size" -lt 50 ]; then
    echo "FAIL: $ref is empty or near-empty ($size bytes)"
    status=1
  else
    echo "ok: $ref ($size bytes)"
  fi
done

echo
echo "== packaged-skill hygiene =="
# Keep skill content portable: no host-specific absolute paths.
bad_abs=$(rg -n --pcre2 '(/home/|/Users/)[^[:space:]]+' \
  "$ROOT"/SKILL.md "$ROOT"/README.md "$ROOT"/references \
  "$ROOT"/conventions "$ROOT"/workflows "$ROOT"/gotchas 2>/dev/null || true)
if [ -n "$bad_abs" ]; then
  echo "FAIL: packaged skill content contains host-specific absolute paths"
  printf '%s\n' "$bad_abs" | sed 's/^/    /'
  status=1
else
  echo "ok: no host-specific absolute paths in skill content"
fi

bad_raw=$(rg -n 'rknn_doc/MinerU_.*_CN\.md\b|rknn_doc/MinerU_.*_EN\.md\b' "$ROOT"/references "$ROOT"/README.md "$ROOT"/SKILL.md 2>/dev/null || true)
if [ -n "$bad_raw" ]; then
  echo "FAIL: exact rknn_doc file references use unstable names without wildcard-safe wording"
  printf '%s\n' "$bad_raw" | sed 's/^/    /'
  status=1
else
  echo "ok: raw-doc references avoid unstable exact filenames"
fi

# Frontmatter sanity: name must match the skill name, description must be one line and <=1024 chars.
name_line=$(awk 'BEGIN{c=0} /^---$/{c++; next} c==1 && /^name:/{print; exit}' "$ROOT/SKILL.md")
desc_line=$(awk 'BEGIN{c=0} /^---$/{c++; next} c==1 && /^description:/{print; exit}' "$ROOT/SKILL.md")
skill_name=$(printf '%s' "$name_line" | sed 's/^name:[[:space:]]*//' | tr -d '\r')
if [ "$skill_name" = "rknn-rk-toolkit2" ]; then
  echo "ok: SKILL.md name is rknn-rk-toolkit2"
else
  echo "FAIL: SKILL.md name missing or does not match the skill name"
  status=1
fi
# Qoder/ZCode/Claude Code/Codex naming: lowercase, digits, hyphens, no spaces/dots, <=64 chars.
if printf '%s' "$skill_name" | rg -q '^[a-z0-9][a-z0-9-]*$' && [ "${#skill_name}" -le 64 ]; then
  echo "ok: skill name satisfies lowercase/hyphen and <=64 chars (host agent naming)"
else
  echo "FAIL: skill name violates lowercase/hyphen or <=64 chars (host agent naming)"
  status=1
fi
if [ -n "$desc_line" ] && [ "${#desc_line}" -le 1024 ]; then
  echo "ok: SKILL.md description is one line (${#desc_line} chars)"
else
  echo "FAIL: SKILL.md description missing, wrapped, or longer than 1024 chars"
  status=1
fi
if rg -q 'TODO:' "$ROOT"/SKILL.md "$ROOT"/README.md "$ROOT"/references "$ROOT"/conventions "$ROOT"/workflows "$ROOT"/gotchas 2>/dev/null; then
  echo "FAIL: unresolved TODO: markers remain"
  status=1
else
  echo "ok: no unresolved TODO: markers"
fi

echo
echo "== routing integrity =="
routing_refs=$(rg -o '(workflows|references|gotchas|conventions)/[A-Za-z0-9._-]+\.md' "$ROOT/routing.yaml" | sort -u)
while IFS= read -r rel; do
  [ -n "$rel" ] || continue
  if [ -f "$ROOT/$rel" ]; then
    echo "ok: routing -> $rel"
  else
    echo "FAIL: routing.yaml points at missing file: $rel"
    status=1
  fi
done <<< "$routing_refs"
while IFS= read -r rel; do
  [ -n "$rel" ] || continue
  if [ -f "$ROOT/$rel" ]; then
    echo "ok: SKILL.md -> $rel"
  else
    echo "FAIL: SKILL.md points at missing file: $rel"
    status=1
  fi
done < <(rg -o '(workflows|references|gotchas|conventions)/[A-Za-z0-9._-]+\.md' "$ROOT/SKILL.md" | sort -u)

echo
echo "== reference activation =="
# Every reference should be reachable from the entry/process layer.
for ref in "$ROOT"/references/*.md; do
  base=$(basename "$ref")
  if rg -q -F "$base" "$ROOT/SKILL.md" "$ROOT/README.md" "$ROOT/routing.yaml" \
      "$ROOT/conventions" "$ROOT/workflows" "$ROOT/gotchas"; then
    echo "ok: $base is linked"
  else
    echo "WARN: $base is not linked from SKILL/README/routing/workflows/conventions/gotchas"
  fi
done

echo
echo "== markdown table integrity =="
if python3 - "$ROOT" <<'PY'
import sys
from pathlib import Path

root = Path(sys.argv[1])
files = [root / "SKILL.md", root / "README.md"]
for d in ("references", "workflows", "gotchas", "conventions"):
    files += sorted((root / d).glob("*.md"))

bad = 0

def pipe_count(line: str) -> int:
    return line.replace("\\|", "\x00").count("|")

for f in files:
    lines = f.read_text(encoding="utf-8").splitlines()
    in_code = False
    block = []
    start = 0
    def check(block, start):
        global bad
        if not block:
            return
        counts = {pipe_count(line) for _, line in block}
        if len(counts) > 1:
            print(f"FAIL: {f.relative_to(root)} table at line {start} has inconsistent column counts {sorted(counts)}")
            bad += 1
    for i, line in enumerate(lines, 1):
        if line.strip().startswith("```"):
            in_code = not in_code
            continue
        if in_code:
            continue
        if line.lstrip().startswith("|"):
            if not block:
                start = i
            block.append((i, line))
        else:
            check(block, start)
            block = []
    check(block, start)

sys.exit(1 if bad else 0)
PY
then
  echo "ok: markdown tables have consistent column counts"
else
  echo "FAIL: markdown table column counts are inconsistent"
  status=1
fi

echo
echo "== skill structure (routing / conventions / workflows / gotchas) =="
for req in \
  "$ROOT/routing.yaml" \
  "$ROOT/agents/openai.yaml" \
  "$ROOT/conventions/agent-response.md" \
  "$ROOT/gotchas/high-cost.md" \
  "$ROOT/workflows/convert-model.md" \
  "$ROOT/workflows/quant-accuracy.md" \
  "$ROOT/workflows/board-debug.md" \
  "$ROOT/workflows/runtime-c.md" \
  "$ROOT/workflows/operator-support.md" \
  "$ROOT/workflows/graph-advanced.md" \
  "$ROOT/workflows/model-zoo-port.md" \
  "$ROOT/workflows/platform-special.md" \
  "$ROOT/workflows/doc-lookup.md" \
  "$ROOT/references/21_custom_op.md" \
  "$ROOT/references/22_onnx_edit_and_graph_tools.md" \
  "$ROOT/references/23_sparsity_and_toolkit_functions.md" \
  "$ROOT/references/24_maintainer_update.md"
do
  if [ -f "$req" ]; then
    size=$(wc -c <"$req")
    if [ "$size" -lt 80 ]; then
      echo "FAIL: $req is empty or near-empty ($size bytes)"
      status=1
    else
      echo "ok: $req ($size bytes)"
    fi
  else
    echo "FAIL: $req missing"
    status=1
  fi
done

if rg -q '## Common Tasks' "$ROOT/SKILL.md" && rg -q 'workflows/convert-model.md' "$ROOT/SKILL.md" && rg -q 'workflows/graph-advanced.md' "$ROOT/SKILL.md"; then
  echo "ok: SKILL.md routes Common Tasks to workflows/ (incl. graph-advanced)"
else
  echo "FAIL: SKILL.md missing Common Tasks routing to workflows/"
  status=1
fi

if rg -q 'references/15_rknn_doc_lookup.md' "$ROOT/SKILL.md"; then
  echo "ok: SKILL.md points at raw-doc lookup"
else
  echo "FAIL: SKILL.md does not point at references/15_rknn_doc_lookup.md"
  status=1
fi

# Dual budget: keep SKILL.md navigable (description short, body not a dump)
skill_body_lines=$(awk 'BEGIN{p=0} /^---$/{c++; next} c>=2{p=1} p{n++} END{print n+0}' "$ROOT/SKILL.md")
if [ "$skill_body_lines" -gt 120 ]; then
  echo "WARN: SKILL.md body has $skill_body_lines lines (prefer ≤90–120 navigable body)"
else
  echo "ok: SKILL.md body line count $skill_body_lines"
fi

lookup_ref="$ROOT/references/15_rknn_doc_lookup.md"
if [ -f "$lookup_ref" ]; then
  size=$(wc -c <"$lookup_ref")
  if [ "$size" -lt 300 ]; then
    echo "FAIL: $lookup_ref is too small ($size bytes)"
    status=1
  else
    echo "ok: $lookup_ref ($size bytes)"
  fi
else
  echo "FAIL: $lookup_ref missing"
  status=1
fi

entry_ref="$ROOT/references/16_task_entry_map.md"
if [ -f "$entry_ref" ]; then
  size=$(wc -c <"$entry_ref")
  if [ "$size" -lt 300 ]; then
    echo "FAIL: $entry_ref is too small ($size bytes)"
    status=1
  else
    echo "ok: $entry_ref ($size bytes)"
  fi
else
  echo "FAIL: $entry_ref missing"
  status=1
fi

contract_ref="$ROOT/references/17_response_contracts.md"
if [ -f "$contract_ref" ]; then
  size=$(wc -c <"$contract_ref")
  if [ "$size" -lt 300 ]; then
    echo "FAIL: $contract_ref is too small ($size bytes)"
    status=1
  else
    echo "ok: $contract_ref ($size bytes)"
  fi
else
  echo "FAIL: $contract_ref missing"
  status=1
fi

review_ref="$ROOT/references/18_review_checklist.md"
if [ -f "$review_ref" ]; then
  size=$(wc -c <"$review_ref")
  if [ "$size" -lt 300 ]; then
    echo "FAIL: $review_ref is too small ($size bytes)"
    status=1
  else
    echo "ok: $review_ref ($size bytes)"
  fi
else
  echo "FAIL: $review_ref missing"
  status=1
fi

lite2_ref="$ROOT/references/19_toolkit_lite2_board_python.md"
if [ -f "$lite2_ref" ]; then
  size=$(wc -c <"$lite2_ref")
  if [ "$size" -lt 300 ]; then
    echo "FAIL: $lite2_ref is too small ($size bytes)"
    status=1
  else
    echo "ok: $lite2_ref ($size bytes)"
  fi
else
  echo "FAIL: $lite2_ref missing"
  status=1
fi

smoke_ref="$ROOT/references/20_official_examples_and_smoke_paths.md"
if [ -f "$smoke_ref" ]; then
  size=$(wc -c <"$smoke_ref")
  if [ "$size" -lt 300 ]; then
    echo "FAIL: $smoke_ref is too small ($size bytes)"
    status=1
  else
    echo "ok: $smoke_ref ($size bytes)"
  fi
else
  echo "FAIL: $smoke_ref missing"
  status=1
fi

echo
echo "== rknn_doc raw docs (V2.3.2 ground truth) =="
if [ -d "$ROOT/rknn_doc" ]; then
  # Verify the README and a few canonical files exist and are non-empty.
  for f in "$ROOT"/rknn_doc/README.md \
           "$ROOT"/rknn_doc/MinerU_markdown_02_Rockchip_RKNPU_User_Guide_*.md \
           "$ROOT"/rknn_doc/MinerU_markdown_05_RKNN_Compiler_Support_Operator_List_*.md; do
    if [ -f "$f" ]; then
      size=$(wc -c <"$f")
      if [ "$size" -lt 100 ]; then
        echo "FAIL: $f is empty or near-empty ($size bytes)"
        status=1
      else
        echo "ok: $f ($size bytes)"
      fi
    else
      echo "FAIL: $f missing"
      status=1
    fi
  done
  # Count markdown files; warn if the directory looks partial.
  n=$(ls "$ROOT"/rknn_doc/*.md 2>/dev/null | wc -l)
  echo "total rknn_doc markdown files: $n"
  if [ "$n" -lt 10 ]; then
    echo "WARN: rknn_doc/ has only $n .md files (expected ~12). Some raw docs may be missing."
  fi
else
  echo "skip (rknn_doc/ not present — V2.3.2 raw layer not bundled)"
fi

echo
if [ "$status" -eq 0 ]; then
  echo "smoke test: PASS"
else
  echo "smoke test: FAIL"
fi
exit "$status"
