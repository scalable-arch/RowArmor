# #!/usr/bin/env python3
# # -*- coding: utf-8 -*-
import os
import re
import sys
import shutil
from pathlib import Path
from typing import Set, Optional, List

ERROR_SNIPPET = "C: Tool (or Pin) caused signal 11"
DEFAULT_SH_ROOTS = [Path("../simulation_scripts"), Path("."), Path("..")]
OUTPUT_SH = Path("rerun_sig11.sh")

def find_bad_outs(start: Path) -> Set[Path]:
    bad = set()
    for root, _, files in os.walk(start):
        for fn in files:
            if not fn.endswith(".out"):
                continue
            p = Path(root) / fn
            try:
                with open(p, "r", errors="ignore") as f:
                    for line in f:
                        if ERROR_SNIPPET in line:
                            bad.add(p.resolve())
                            break
            except Exception:
                pass
    return bad

def iter_command_blocks(sh_text: str) -> List[str]:
    
    blocks = []
    for m in re.finditer(r'commands\s*=\s*\(\s*(.*?)\s*\)', sh_text, flags=re.DOTALL):
        blocks.append(m.group(1))
    return blocks

def extract_commands_from_block(block: str) -> List[str]:
    cmds = []
    
    for s in re.finditer(r'"([^"\\]*(?:\\.[^"\\]*)*)"|\'([^\'\\]*(?:\\.[^\'\\]*)*)\'', block, flags=re.DOTALL):
        item = s.group(1) if s.group(1) is not None else s.group(2)
        item = item.replace('\\"', '"').replace("\\'", "'")
        cmds.append(item)
    return cmds

def get_out_path_from_command(cmd: str) -> Optional[str]:

    cands = re.findall(r'(?:\d?>|>>)\s*([^\s]+\.out)', cmd)
    if cands:
        return cands[-1]
    m = re.search(r'2>&1\s*>\s*([^\s]+\.out)', cmd)
    if m:
        return m.group(1)
    return None

def norm_suffix(s: str) -> str:

    s = s.replace("\\", "/")
    while s.startswith("./"):
        s = s[2:]
    while s.startswith("../"):
        s = s[3:]
    return s

def path_suffix_match(abs_path: Path, rel_str: str) -> bool:
    suffix = norm_suffix(rel_str)
    ap = abs_path.as_posix()
    return ap.endswith("/" + suffix) or ap == suffix

def discover_sh_files(roots: List[Path]) -> List[Path]:
    sh_files = []
    seen = set()
    for r in roots:
        if not r.exists():
            continue
        for p in r.rglob("*.sh"):
            if p.is_file():
                rp = p.resolve()
                if rp not in seen:
                    sh_files.append(rp)
                    seen.add(rp)
    return sorted(sh_files)

def main():
    cwd = Path(".").resolve()
    bad_outs = find_bad_outs(cwd)
    print(f"[info] SIG11 .out files {len(bad_outs)} detected:")
    for p in sorted(bad_outs):
        print("  -", p)

    if len(sys.argv) > 1 and sys.argv[1] == "--sh-root":
        if len(sys.argv) < 3:
            print("[err] --sh-root directory needed.", file=sys.stderr)
            sys.exit(2)
        roots = [Path(sys.argv[2])]
    else:
        roots = DEFAULT_SH_ROOTS

    sh_files = discover_sh_files(roots)
    if not sh_files:
        print(f"[warn] can't not find shell scripts. roots={', '.join(str(r) for r in roots)}", file=sys.stderr)

    selected_cmds = []
    seen_cmds = set()

    unmatched_outs = set(bad_outs)  

    for sh in sh_files:
        try:
            text = sh.read_text(errors="ignore")
        except Exception:
            continue

        blocks = iter_command_blocks(text)
        if not blocks:
            continue

        sh_dir = sh.parent.resolve()

        for block in blocks:
            cmds = extract_commands_from_block(block)
            for cmd in cmds:
                out_rel = get_out_path_from_command(cmd)
                if not out_rel:
                    continue

                out_abs1 = (sh_dir / out_rel).resolve()
                hit = None
                if out_abs1 in bad_outs:
                    hit = out_abs1
                else:
                    out_abs2 = (cwd / out_rel).resolve()
                    if out_abs2 in bad_outs:
                        hit = out_abs2
                    else:
                        for bo in bad_outs:
                            if path_suffix_match(bo, out_rel):
                                hit = bo
                                break

                if hit is not None:
                    if cmd not in seen_cmds:
                        selected_cmds.append(cmd)
                        seen_cmds.add(cmd)
                    if hit in unmatched_outs:
                        unmatched_outs.remove(hit)

    if not selected_cmds:
        print("[info] No cmd to rerun.")
        if unmatched_outs:
            for p in sorted(unmatched_outs):
                print("  -", p)
        return

    lines = []
    lines.append("#!/usr/bin/env bash")
    lines.append("")
    lines.append("ulimit -s unlimited")
    lines.append("ulimit -v unlimited")
    lines.append("ulimit -m unlimited")
    lines.append("ulimit -n 65535")
    lines.append("")
    # === SIGINT handler ===
    lines.append("cleanup_on_interrupt() {")
    lines.append("    echo \"\"")
    lines.append("    echo \"[INTERRUPT] Ctrl+C detected. Killing all 'ls' processes.\"")
    lines.append("    killall -9 ls || true")
    lines.append("    echo \"[INTERRUPT] Cleaning up background jobs...\"")
    lines.append("    jobs -p | xargs -r kill -9 2>/dev/null")
    lines.append("    exit 130")
    lines.append("}")
    lines.append("trap cleanup_on_interrupt INT")
    lines.append("")
    lines.append("run_cmd() {")
    lines.append('    local cmd="$1"')
    lines.append('    echo "▶ Running: $cmd"')
    lines.append('    eval "$cmd" &')
    lines.append('    echo $!')
    lines.append("}")
    lines.append("")
    lines.append("commands=(")
    for c in selected_cmds:
        lines.append(f'  "{c}"')
    lines.append(")")
    lines.append("")
    lines.append("batch_size=4")
    lines.append('total=${#commands[@]}')
    lines.append("")
    lines.append('for ((i=0; i<total; i+=batch_size)); do')
    lines.append('    batch_id=$((i/batch_size+1))')
    lines.append('    unset pids outfiles cmds')
    lines.append('    declare -a pids outfiles cmds')
    lines.append("")
    lines.append('    for ((j=0; j<batch_size && i+j<total; j++)); do')
    lines.append('        cmd="${commands[i+j]}"')
    lines.append('        cmds[j]="$cmd"')
    lines.append('        outfiles[j]="${cmd##*> }"')
    lines.append('        echo "▶ Launching: $cmd"')
    lines.append('        eval "$cmd" &')
    lines.append('        pids[j]=$!')
    lines.append('    done')
    lines.append("")
    lines.append('    sleep 180')
    lines.append("")
    lines.append('    for idx in "${!pids[@]}"; do')
    lines.append('        pid=${pids[idx]}')
    lines.append('        outfile=${outfiles[idx]}')
    lines.append('        if grep -q "C: Tool (or Pin) caused signal 11" "$outfile"; then')
    lines.append('            echo "⚠️ Pin error detected in $outfile (PID $pid). Killing it."')
    lines.append('            kill -9 $pid')
    lines.append('        else')
    lines.append('            if kill -0 $pid 2>/dev/null; then')
    lines.append('                wait $pid')
    lines.append('            fi')
    lines.append('        fi')
    lines.append('    done')
    lines.append("")
    lines.append('    killall -9 ls || true')
    lines.append('done')
    lines.append("")


    OUTPUT_SH.write_text("\n".join(lines), encoding="utf-8")
    os.chmod(OUTPUT_SH, 0o755)

    dst = Path("../simulation_scripts") / OUTPUT_SH.name
    shutil.copy(OUTPUT_SH, dst)
    print(f"[ok] rerun script copied to: {dst}")

    print(f"[ok] re-run script generated: {OUTPUT_SH.resolve()}")
    print(f"[sum] matched command: {len(selected_cmds)} / .out: {len(bad_outs)}")
    if unmatched_outs:
        print("[warn] can't not find .out lists:")
        for p in sorted(unmatched_outs):
            print("  -", p)

if __name__ == "__main__":
    main()
