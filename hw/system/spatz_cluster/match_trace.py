#!/usr/bin/env python3
"""
match_trace.py - matches a Snitch .dasm trace log against an annotated .s file
(produced by `make annotate`) and writes, for every retired instruction:
time, cycle number, stall cycles before it, address, instruction text,
and - if available - the originating source line.

By default the report is written to a text file (report.txt), not printed
to the console. Use --stdout to print to console instead.

Usage:
    python3 match_trace.py trace_hart_00000000.txt annotated.s
    python3 match_trace.py trace_hart_00000000.txt annotated.s --out my_report.txt --csv out.csv --top 20
"""

import argparse
import csv
import re
import sys
from collections import namedtuple

# --------------------------------------------------------------------------
# 1. Parse annotated .s file: address -> (asm_text, source_line)
# --------------------------------------------------------------------------

ASM_LINE_RE = re.compile(
    r'^\s*([0-9a-fA-F]+):\s+([0-9a-fA-F ]+?)\s+(\S.*)$'
)

SRC_LINE_RE = re.compile(r'^\s*;\s*(.*)$')


def parse_annotated(path):
    """Return dict: address(int) -> {'asm': str, 'src': str|None}"""
    addr_map = {}
    last_src = None
    with open(path, 'r', errors='replace') as f:
        for line in f:
            src_m = SRC_LINE_RE.match(line)
            if src_m:
                last_src = src_m.group(1).strip()
                continue

            asm_m = ASM_LINE_RE.match(line)
            if asm_m:
                addr_hex, _bytes, asm_text = asm_m.groups()
                try:
                    addr = int(addr_hex, 16)
                except ValueError:
                    continue
                addr_map[addr] = {
                    'asm': asm_text.strip(),
                    'src': last_src,
                }
                last_src = None
    return addr_map


# --------------------------------------------------------------------------
# 2. Parse .dasm trace
# --------------------------------------------------------------------------

TRACE_LINE_RE = re.compile(
    r'^\s*(\d+)\s+\d+\s+\d+\s+(0x[0-9a-fA-F]+)\s+DASM\(([0-9a-fA-F]+)\)\s*#;\s*(\{.*\})?'
)

TraceEntry = namedtuple(
    'TraceEntry', ['time', 'cycle', 'pc', 'encoding', 'fields']
)


def parse_dasm_field_dict(raw):
    """'{'stall': 0x0, 'rd': 0x1c, ...}' -> {'stall': 0, 'rd': 28, ...}"""
    if not raw:
        return {}
    fields = {}
    for m in re.finditer(r"'(\w+)':\s*(0x[0-9a-fA-F]+|-?\d+)", raw):
        key, val = m.groups()
        fields[key] = int(val, 16) if val.startswith('0x') else int(val)
    return fields


def parse_trace(path, time_per_cycle=1000):
    """Return list of TraceEntry; time is divided by time_per_cycle to get cycle number."""
    entries = []
    with open(path, 'r', errors='replace') as f:
        for line in f:
            m = TRACE_LINE_RE.match(line)
            if not m:
                continue
            time_s, pc_s, enc_s, dict_s = m.groups()
            time = int(time_s)
            pc = int(pc_s, 16)
            fields = parse_dasm_field_dict(dict_s)
            entries.append(TraceEntry(
                time=time,
                cycle=time // time_per_cycle,
                pc=pc,
                encoding=enc_s,
                fields=fields,
            ))
    return entries


# --------------------------------------------------------------------------
# 3. Merge and report
# --------------------------------------------------------------------------

def build_report(entries, addr_map):
    rows = []
    prev_cycle = None
    for e in entries:
        info = addr_map.get(e.pc)
        asm_text = info['asm'] if info else f'DASM({e.encoding}) (not found in .s)'
        src_text = info['src'] if info else None

        if prev_cycle is None:
            stall = 0
        else:
            stall = max(0, e.cycle - prev_cycle - 1)
        prev_cycle = e.cycle

        rows.append({
            'time': e.time,
            'cycle': e.cycle,
            'stall_cycles': stall,
            'pc': f'0x{e.pc:08x}',
            'asm': asm_text,
            'src': src_text or '',
        })
    return rows


def write_report(rows, out, show_src=True):
    for r in rows:
        stall_mark = f'  [STALL {r["stall_cycles"]}]' if r['stall_cycles'] else ''
        line = f'{r["time"]:>10} (cyc {r["cycle"]:>6}) {r["pc"]}  {r["asm"]}{stall_mark}'
        out.write(line + '\n')
        if show_src and r['src']:
            out.write(f'{"":>10}          -- {r["src"]}\n')


def write_top_stalls(rows, out, top_n=20):
    biggest = sorted(rows, key=lambda r: r['stall_cycles'], reverse=True)[:top_n]
    out.write(f'\n=== TOP-{top_n} largest stalls ===\n')
    for r in biggest:
        if r['stall_cycles'] == 0:
            continue
        line = (f'{r["stall_cycles"]:>6} cycles  |  cyc {r["cycle"]:>6}  |  '
                f'{r["pc"]}  {r["asm"]}')
        if r['src']:
            line += f'   -- {r["src"]}'
        out.write(line + '\n')


def save_csv(rows, path):
    with open(path, 'w', newline='') as f:
        writer = csv.DictWriter(f, fieldnames=['time', 'cycle', 'stall_cycles', 'pc', 'asm', 'src'])
        writer.writeheader()
        writer.writerows(rows)


# --------------------------------------------------------------------------
# main
# --------------------------------------------------------------------------

def main():
    ap = argparse.ArgumentParser()
    ap.add_argument('trace_file', help='trace_hart_XXXXXXXX.txt (from make traces)')
    ap.add_argument('annotated_file', help='annotated .s file (from make annotate)')
    ap.add_argument('--out', default='report.txt',
                     help='text file to write the full report to (default: report.txt)')
    ap.add_argument('--csv', help='also save the result to a CSV file')
    ap.add_argument('--top', type=int, default=20, help='how many largest stalls to show')
    ap.add_argument('--no-src', action='store_true', help='do not print source lines')
    ap.add_argument('--stdout', action='store_true',
                     help='print the report to the console instead of a text file')
    ap.add_argument('--time-per-cycle', type=int, default=1000,
                     help='time units per cycle in the trace (default 1000)')
    args = ap.parse_args()

    addr_map = parse_annotated(args.annotated_file)
    if not addr_map:
        print('Warning: no instructions parsed from the .s file', file=sys.stderr)

    entries = parse_trace(args.trace_file, time_per_cycle=args.time_per_cycle)
    if not entries:
        print('Warning: no lines parsed from the trace file', file=sys.stderr)
        sys.exit(1)

    rows = build_report(entries, addr_map)

    if args.stdout:
        write_report(rows, sys.stdout, show_src=not args.no_src)
        write_top_stalls(rows, sys.stdout, top_n=args.top)
    else:
        with open(args.out, 'w') as f:
            write_report(rows, f, show_src=not args.no_src)
            write_top_stalls(rows, f, top_n=args.top)
        print(f'Report written to {args.out}')

    if args.csv:
        save_csv(rows, args.csv)
        print(f'CSV saved to {args.csv}')


if __name__ == '__main__':
    main()