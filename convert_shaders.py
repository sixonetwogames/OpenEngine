#!/usr/bin/env python3
"""
convert_shaders.py — Auto-convert GLSL 330 shaders to GLSL ES 100.

Handles ~90% of conversions automatically. Logs warnings for constructs
that need manual attention (array initializers, etc).

Usage:
    python convert_shaders.py assets/shaders/gl330 assets/shaders/es100

Can be wired into CMake as a pre-build step for web builds.
"""

import sys, os, re, shutil
from pathlib import Path

def convert_shader(src: str, is_vertex: bool) -> tuple[str, list[str]]:
    warnings = []
    lines = src.replace('\r\n', '\n').split('\n')
    out = []
    frag_output_var = None
    needs_derivatives = 'dFdx' in src or 'dFdy' in src

    for line in lines:
        stripped = line.strip()

        # --- #version → ES 100 + precision ---
        if stripped.startswith('#version'):
            out.append('#version 100')
            if needs_derivatives and not is_vertex:
                out.append('#extension GL_OES_standard_derivatives : enable')
            out.append('precision mediump float;')
            out.append('precision mediump int;') 
            continue

        # --- Vertex: in → attribute, out → varying ---
        if is_vertex:
            if re.match(r'^(flat\s+)?in\s+', stripped):
                line = re.sub(r'^(\s*)(flat\s+)?in\s+', r'\1attribute ', line)
            elif re.match(r'^(flat\s+)?out\s+', stripped):
                line = re.sub(r'^(\s*)(flat\s+)?out\s+', r'\1varying ', line)

        # --- Fragment: in → varying, out vec4 → track & remove ---
        else:
            if re.match(r'^(flat\s+)?in\s+', stripped):
                line = re.sub(r'^(\s*)(flat\s+)?in\s+', r'\1varying ', line)
            elif re.match(r'^out\s+vec4\s+(\w+)\s*;', stripped):
                m = re.match(r'^out\s+vec4\s+(\w+)\s*;', stripped)
                frag_output_var = m.group(1)
                continue  # remove the line

        # --- texture() → texture2D() ---
        # Avoid matching texture2D that's already correct
        line = re.sub(r'\btexture\s*\(', 'texture2D(', line)
        # Fix double conversion: texture2D2D → texture2D
        line = line.replace('texture2D2D', 'texture2D')

        # --- Replace fragment output variable with gl_FragColor ---
        if not is_vertex and frag_output_var:
            line = line.replace(frag_output_var, 'gl_FragColor')

        # --- ivec2() casts used with gl_FragCoord → vec2 is fine in ES 100 ---
        # (Bayer functions should accept vec2 in ES100 versions)

        out.append(line)

    # --- Warn about constructs that need manual work ---
    if re.search(r'\b\w+\s*\[\s*\d+\s*\]\s*=\s*\w+\s*\[', src):
        warnings.append('Array initializer detected — needs manual conversion to if/else or vec4 lookups')

    if re.search(r'\bivec[234]\b', src):
        warnings.append('ivec type used — ES 100 has limited int support, verify usage')

    return '\n'.join(out), warnings


def process_directory(src_dir: str, dst_dir: str):
    src_path = Path(src_dir)
    dst_path = Path(dst_dir)
    dst_path.mkdir(parents=True, exist_ok=True)

    if not src_path.exists():
        print(f"ERROR: Source directory '{src_dir}' not found")
        sys.exit(1)

    total, warn_count = 0, 0

    for shader_file in sorted(src_path.glob('*.[vf]s')):
        is_vertex = shader_file.suffix == '.vs'
        src_text = shader_file.read_text(encoding='utf-8')

        converted, warnings = convert_shader(src_text, is_vertex)

        out_file = dst_path / shader_file.name
        out_file.write_text(converted, encoding='utf-8')
        total += 1

        status = 'OK' if not warnings else 'WARN'
        print(f"  [{status}] {shader_file.name}")
        for w in warnings:
            print(f"         ⚠ {w}")
            warn_count += 1

    print(f"\nConverted {total} shaders. {warn_count} warning(s).")
    if warn_count:
        print("Review warnings above — these need manual fixes in the es100/ output.")


if __name__ == '__main__':
    if len(sys.argv) != 3:
        print(f"Usage: {sys.argv[0]} <gl330_dir> <es100_dir>")
        sys.exit(1)
    process_directory(sys.argv[1], sys.argv[2])
