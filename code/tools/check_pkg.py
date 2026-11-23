"""统计源码注释率，并检查提交文件与 PDF 的匿名合规性。

用法（在仓库根执行）：
    python code/tools/check_pkg.py
    python code/tools/check_pkg.py --pack <输出zip路径>
"""
import argparse
import os
import re
import subprocess
import sys
import zipfile

ROOT = os.path.dirname(os.path.dirname(os.path.dirname(os.path.abspath(__file__))))
SRC_DIR = os.path.join(ROOT, "code", "src")


def count_comments(path):
    """统计总行数、非空行数、以及含注释的行数（同时识别 // 与 /* ... */ 块注释）。"""
    total = 0
    nonempty = 0
    commented = 0
    in_block = False
    with open(path, "r", encoding="utf-8") as f:
        for raw in f:
            total += 1
            line = raw.strip()
            if not line:
                continue
            nonempty += 1
            has_cmt = False
            i = 0
            n = len(line)
            in_str = False
            while i < n:
                ch = line[i]
                if in_block:
                    has_cmt = True
                    if ch == "*" and i + 1 < n and line[i + 1] == "/":
                        in_block = False
                        i += 2
                        continue
                    i += 1
                    continue
                if ch == '"' and (i == 0 or line[i - 1] != "\\"):
                    in_str = not in_str
                    i += 1
                    continue
                if not in_str:
                    if ch == "/" and i + 1 < n and line[i + 1] == "/":
                        has_cmt = True
                        break
                    if ch == "/" and i + 1 < n and line[i + 1] == "*":
                        has_cmt = True
                        in_block = True
                        i += 2
                        continue
                i += 1
            if has_cmt:
                commented += 1
    return total, nonempty, commented


def check_comment_ratio():
    print("=== 源码注释率统计 ===")
    tot_all = non_all = cmt_all = 0
    for fn in sorted(os.listdir(SRC_DIR)):
        if not fn.endswith((".h", ".cpp", ".c")):
            continue
        p = os.path.join(SRC_DIR, fn)
        t, ne, cm = count_comments(p)
        tot_all += t
        non_all += ne
        cmt_all += cm
        print(f"  {fn:<14} total={t:4d}  nonempty={ne:4d}  comment_lines={cm:4d}  ratio_total={cm/t*100:5.1f}%  ratio_nonempty={cm/ne*100:5.1f}%")
    r_tot = cmt_all / tot_all * 100
    r_ne = cmt_all / non_all * 100
    print(f"  {'TOTAL':<14} total={tot_all:4d}  nonempty={non_all:4d}  comment_lines={cmt_all:4d}  ratio_total={r_tot:5.1f}%  ratio_nonempty={r_ne:5.1f}%")
    if r_tot < 30.0:
        print("[WARN] 按总行数计注释率低于 30%！", file=sys.stderr)
        return False
    return True


def build_pr_zip(out_zip):
    """生成 PR 互评阶段匿名提交包，顶层只包含 documents/ 与 code/。"""
    os.makedirs(os.path.dirname(os.path.abspath(out_zip)), exist_ok=True)
    pdf_src = os.path.join(ROOT, "documents", "report.pdf")
    if not os.path.exists(pdf_src):
        print(f"[ERROR] 找不到 {pdf_src}，请先编译报告", file=sys.stderr)
        sys.exit(1)

    with zipfile.ZipFile(out_zip, "w", compression=zipfile.ZIP_DEFLATED) as zf:
        # 1. documents/：readme.txt + 匿名命名 PDF + results/（不含 .tex 与 build/）
        zf.write(os.path.join(ROOT, "documents", "readme.txt"), "documents/readme.txt")
        zf.write(pdf_src, "documents/wc_2026G##_P5.pdf")
        res_dir = os.path.join(ROOT, "documents", "results")
        if os.path.isdir(res_dir):
            for dp, _, fns in os.walk(res_dir):
                for fn in sorted(fns):
                    full = os.path.join(dp, fn)
                    rel = os.path.relpath(full, os.path.join(ROOT, "documents")).replace("\\", "/")
                    zf.write(full, f"documents/{rel}")

        # 2. code/：Readme.txt + src/ + samples/ + tools/（排除 __pycache__、*.exe）
        code_dir = os.path.join(ROOT, "code")
        for dp, dnames, fns in os.walk(code_dir):
            dnames[:] = sorted(d for d in dnames if d != "__pycache__")
            for fn in sorted(fns):
                if fn.endswith((".exe", ".o", ".obj", ".pyc")):
                    continue
                full = os.path.join(dp, fn)
                rel = os.path.relpath(full, code_dir).replace("\\", "/")
                zf.write(full, f"code/{rel}")

    print(f"已生成 PR 匿名提交包: {out_zip} ({os.path.getsize(out_zip)} bytes)")


def main():
    ap = argparse.ArgumentParser()
    ap.add_argument("--pack", default="", help="输出 PR 匿名提交包路径")
    args = ap.parse_args()

    ok = check_comment_ratio()
    if args.pack:
        build_pr_zip(args.pack)
    if not ok:
        sys.exit(1)


if __name__ == "__main__":
    main()
