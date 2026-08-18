#!/usr/bin/env python3
"""Decode RK0 QEMU trace overflow frames and write an HTML timeline.

Input is a QEMU/stdout log containing lines like:

    KTRACE_FRAME 524b545201012e00...

The hex text decodes to a little-endian binary frame:

    magic[4]      "RKTR"
    version       u8, currently 1
    kind          u8, 1 object history, 2 task priority, 3 task overrun
    length        u16, total frame bytes including checksum
    sequence      u32
    dropped       u32, overflow records dropped before this frame
    subject_ptr   u32
    name          char[8], NUL padded
    actor_cycle   u32 in each payload
    actor_name    char[8], NUL padded in each payload
    payload       kind-specific
    checksum      u16, sum(frame without checksum) & 0xffff
"""

from __future__ import annotations

import argparse
import html
import json
import re
import struct
from dataclasses import dataclass
from pathlib import Path
from typing import Iterable


PREFIX = "KTRACE_FRAME "
MAGIC = b"RKTR"
VERSION = 1
KIND_OBJECT = 1
KIND_TASK_PRIO = 2
KIND_TASK_OVERRUN = 3

OBJ_TYPES = {
    0xD00FFF01: "sema",
    0xD00FFF02: "sleepq",
    0xD00FFF04: "mutex",
    0xD01FFF01: "mesgq",
    0xD01FFF02: "mrm",
    0xD01FFF04: "chan",
    0xD01FFF05: "rdvz",
    0xD02FFF01: "timer",
    0xD04FFF01: "mem",
    0xD08FFF01: "task",
}

OPS = {
    1: "init",
    2: "name",
    3: "query",
    4: "alloc",
    5: "free",
    6: "send",
    7: "recv",
    8: "jam",
    9: "post",
    10: "pend",
    11: "block",
    12: "wake",
    13: "timeout",
    14: "reset",
    15: "lock",
    16: "unlock",
    17: "call",
    18: "accept",
    19: "done",
    20: "reserve",
    21: "publish",
    22: "get",
    23: "unget",
    24: "cancel",
    25: "reload",
    26: "expire",
    27: "sendblk",
    28: "recvblk",
    29: "jamblk",
    30: "pendblk",
    31: "lockblk",
    32: "wait",
    33: "waitblk",
    34: "overrun",
}

OVERRUN_KINDS = {
    1: "release",
    2: "until",
}


@dataclass(frozen=True)
class Event:
    kind: str
    seq: int
    dropped: int
    ptr: int
    name: str
    actor_name: str
    tick: int
    lane: str
    label: str
    detail: dict[str, int | str]


def u16(data: bytes, offset: int) -> int:
    return struct.unpack_from("<H", data, offset)[0]


def u32(data: bytes, offset: int) -> int:
    return struct.unpack_from("<I", data, offset)[0]


def i16(data: bytes, offset: int) -> int:
    return struct.unpack_from("<h", data, offset)[0]


def clean_name(raw: bytes) -> str:
    return raw.split(b"\x00", 1)[0].decode("ascii", errors="replace") or "-"


def decode_frame(data: bytes) -> Event:
    if len(data) < 30:
        raise ValueError("frame too short")
    if data[0:4] != MAGIC:
        raise ValueError("bad magic")
    if data[4] != VERSION:
        raise ValueError(f"unsupported version {data[4]}")

    kind = data[5]
    length = u16(data, 6)
    if length != len(data):
        raise ValueError(f"length mismatch frame={length} actual={len(data)}")

    expected = u16(data, len(data) - 2)
    actual = sum(data[:-2]) & 0xFFFF
    if expected != actual:
        raise ValueError(f"checksum mismatch frame={expected} actual={actual}")

    seq = u32(data, 8)
    dropped = u32(data, 12)
    ptr = u32(data, 16)
    name = clean_name(data[20:28])

    if kind == KIND_OBJECT:
        if len(data) != 58:
            raise ValueError(f"bad object frame length {len(data)}")
        obj_id = u32(data, 28)
        tick = u32(data, 32)
        actor_cycle = u32(data, 36)
        value = u32(data, 40)
        result = i16(data, 44)
        op_id = data[46]
        actor_pid = data[47]
        actor_name = clean_name(data[48:56])
        obj_type = OBJ_TYPES.get(obj_id, f"0x{obj_id:08x}")
        op = OPS.get(op_id, f"op{op_id}")
        lane = f"{obj_type}/{name}@0x{ptr:08x}"
        detail: dict[str, int | str] = {
            "obj_id": f"0x{obj_id:08x}",
            "obj_type": obj_type,
            "op_id": op_id,
            "op": op,
            "result": result,
            "value": value,
            "actor_pid": actor_pid,
            "actor_cycle": actor_cycle,
            "actor_name": actor_name,
        }
        return Event("object", seq, dropped, ptr, name, actor_name, tick, lane, op, detail)

    if kind == KIND_TASK_PRIO:
        if len(data) != 51:
            raise ValueError(f"bad task-prio frame length {len(data)}")
        tick = u32(data, 28)
        actor_cycle = u32(data, 32)
        pid = data[36]
        actor_pid = data[37]
        old = data[38]
        new = data[39]
        nominal = data[40]
        actor_name = clean_name(data[41:49])
        lane = f"task/{name}@0x{ptr:08x}"
        label = f"{old}->{new}"
        detail = {
            "pid": pid,
            "actor_pid": actor_pid,
            "actor_cycle": actor_cycle,
            "actor_name": actor_name,
            "old": old,
            "new": new,
            "nominal": nominal,
        }
        return Event("task_prio", seq, dropped, ptr, name, actor_name, tick, lane, label, detail)

    if kind == KIND_TASK_OVERRUN:
        if len(data) != 64:
            raise ValueError(f"bad task-overrun frame length {len(data)}")
        tick = u32(data, 28)
        actor_cycle = u32(data, 32)
        actor_pid = data[36]
        overrun_id = data[37]
        actor_name = clean_name(data[38:46])
        period = u32(data, 46)
        late_by = u32(data, 50)
        skipped = u32(data, 54)
        total = u32(data, 58)
        overrun_kind = OVERRUN_KINDS.get(overrun_id, f"kind{overrun_id}")
        lane = f"task/{name}@0x{ptr:08x}"
        label = f"{overrun_kind} ovr"
        detail = {
            "pid": actor_pid,
            "actor_pid": actor_pid,
            "actor_cycle": actor_cycle,
            "actor_name": actor_name,
            "overrun_id": overrun_id,
            "overrun_kind": overrun_kind,
            "period": period,
            "late_by": late_by,
            "skipped": skipped,
            "total": total,
        }
        return Event("task_overrun", seq, dropped, ptr, name, actor_name, tick, lane, label, detail)

    raise ValueError(f"unknown kind {kind}")


def iter_frame_hex(lines: Iterable[str]) -> Iterable[tuple[int, str]]:
    pattern = re.compile(r"KTRACE_FRAME\s+([0-9a-fA-F]+)")
    for line_no, line in enumerate(lines, 1):
        match = pattern.search(line)
        if match:
            yield line_no, match.group(1)


def load_events(path: Path) -> tuple[list[Event], list[str]]:
    events: list[Event] = []
    errors: list[str] = []
    with path.open("r", encoding="utf-8", errors="replace") as handle:
        for line_no, hex_text in iter_frame_hex(handle):
            try:
                events.append(decode_frame(bytes.fromhex(hex_text)))
            except ValueError as exc:
                errors.append(f"line {line_no}: {exc}")
    events.sort(key=lambda event: (event.tick, event.seq))
    return events, errors


def lane_order(events: list[Event]) -> list[tuple[str, str, str]]:
    """Return lanes as (key, display, class), with tasks before objects."""
    task_first: dict[str, int] = {}
    object_first: dict[str, int] = {}
    task_overruns: dict[str, int] = {}

    for event in events:
        if event.actor_name != "-":
            task_key = f"task/{event.actor_name}"
            task_first.setdefault(task_key, event.seq)
            if event.kind == "task_overrun":
                task_overruns[task_key] = max(
                    task_overruns.get(task_key, 0),
                    int(event.detail.get("total", 0)),
                )
        if event.kind == "task_prio":
            task_first.setdefault(event.lane, event.seq)
        elif event.kind == "object":
            object_first.setdefault(event.lane, event.seq)

    lanes: list[tuple[str, str, str]] = []
    for key in sorted(task_first, key=lambda lane: task_first[lane]):
        overruns = task_overruns.get(key, 0)
        display = f"{key}  ovr={overruns}" if overruns else key
        lanes.append((key, display, "task"))
    for key in sorted(object_first, key=lambda lane: object_first[lane]):
        lanes.append((key, key, "object"))
    return lanes


def event_lanes(event: Event) -> list[tuple[str, str]]:
    lanes: list[tuple[str, str]] = []
    if event.actor_name != "-":
        lanes.append((f"task/{event.actor_name}", event.label))
    if event.kind == "task_prio":
        lanes.append((event.lane, f"prio {event.label}"))
    elif event.kind == "object":
        lanes.append((event.lane, event.label))
    return lanes


def event_color(event: Event) -> str:
    if event.dropped:
        return "#d23f31"
    if event.kind == "task_overrun":
        return "#b22d2d"
    if event.kind == "task_prio":
        return "#7a4cc2"
    op = str(event.detail.get("op", ""))
    if op in {"send", "recv", "call", "accept", "done"}:
        return "#2b7a78"
    if op in {"block", "sendblk", "recvblk", "pendblk", "lockblk", "waitblk"}:
        return "#c46a1a"
    if op in {"timeout"}:
        return "#a43f52"
    if op in {"alloc", "free"}:
        return "#3e6ca8"
    return "#30343b"


def render_html(events: list[Event], errors: list[str], title: str) -> str:
    lanes = lane_order(events)
    min_tick = min((event.tick for event in events), default=0)
    max_tick = max((event.tick for event in events), default=1)
    if min_tick == max_tick:
        max_tick = min_tick + 1

    tick_span = max_tick - min_tick
    px_per_tick = 18 if tick_span <= 120 else 10 if tick_span <= 500 else 5
    chart_w = max(1200, tick_span * px_per_tick + 360)
    base_row_h = 42
    event_h = 26
    level_gap = 30
    lane_keys = [lane[0] for lane in lanes]

    def x_for(tick: int) -> int:
        return 28 + ((tick - min_tick) * px_per_tick)

    events_by_lane: dict[str, list[tuple[Event, str]]] = {key: [] for key in lane_keys}
    for event in events:
        for lane_key, label in event_lanes(event):
            if lane_key in events_by_lane:
                events_by_lane[lane_key].append((event, label))

    tick_step = max(1, round(max(1, tick_span) / 12))
    ticks = list(range(min_tick, max_tick + 1, tick_step))
    if ticks[-1] != max_tick:
        ticks.append(max_tick)

    tick_marks = []
    for tick in ticks:
        left = x_for(tick)
        tick_marks.append(
            f'<div class="tick" style="left:{left}px"><span>{tick}</span></div>'
        )

    row_html = []
    for lane_key, display, klass in lanes:
        lane_events = events_by_lane.get(lane_key, [])
        lane_events.sort(key=lambda item: (item[0].tick, item[0].seq))
        blocks = []
        level_right: list[int] = []
        for idx, (event, label) in enumerate(lane_events):
            left = x_for(event.tick)
            next_left = (
                x_for(lane_events[idx + 1][0].tick)
                if idx + 1 < len(lane_events)
                else left + 70
            )
            width = max(26, min(180, next_left - left - 3))
            level = 0
            while level < len(level_right) and left < level_right[level] + 4:
                level += 1
            if level == len(level_right):
                level_right.append(left + width)
            else:
                level_right[level] = left + width
            top = 8 + (level * level_gap)
            color = event_color(event)
            detail = {
                "seq": event.seq,
                "tick": event.tick,
                "lane": lane_key,
                "subject": event.lane,
                "actor_name": event.actor_name,
                "label": label,
                "dropped": event.dropped,
                **event.detail,
            }
            detail_json = html.escape(json.dumps(detail, sort_keys=True), quote=True)
            subject = html.escape(event.lane, quote=True)
            drop = (
                f'<span class="drop-badge">+{event.dropped} lost</span>'
                if event.dropped
                else ""
            )
            blocks.append(
                f'<button class="event-block {event.kind}" '
                f'style="left:{left}px;top:{top}px;width:{width}px;'
                f'--event-color:{color}" '
                f'data-subject="{subject}" data-detail="{detail_json}">'
                f'<span>{html.escape(label)}</span>{drop}</button>'
            )
        blocks_html = "".join(blocks)
        row_h = max(base_row_h, 14 + max(1, len(level_right)) * level_gap)
        row_html.append(
            f'<div class="trace-row {klass}" data-lane="{html.escape(lane_key, quote=True)}" '
            f'style="min-height:{row_h}px">'
            f'<div class="lane-label"><span>{html.escape(display)}</span></div>'
            f'<div class="track" style="width:{chart_w}px;height:{row_h}px">'
            f'{blocks_html}</div>'
            '</div>'
        )

    dropped_total = sum(event.dropped for event in events)
    object_count = sum(1 for event in events if event.kind == "object")
    prio_count = sum(1 for event in events if event.kind == "task_prio")
    overrun_count = sum(1 for event in events if event.kind == "task_overrun")
    lane_count = len(lanes)
    grid_step = max(px_per_tick, 1)
    legend = "".join(
        [
            '<span><i style="background:#2b7a78"></i>message</span>',
            '<span><i style="background:#c46a1a"></i>blocking</span>',
            '<span><i style="background:#3e6ca8"></i>memory</span>',
            '<span><i style="background:#7a4cc2"></i>priority</span>',
            '<span><i style="background:#b22d2d"></i>overrun</span>',
            '<span><i style="background:#a43f52"></i>timeout/drop</span>',
        ]
    )
    object_options = ['<option value="__all__">All kernel objects</option>']
    for lane_key, display, klass in lanes:
        if klass == "object":
            escaped = html.escape(lane_key, quote=True)
            object_options.append(
                f'<option value="{escaped}">{html.escape(display)}</option>'
            )

    error_items = "".join(f"<li>{html.escape(err)}</li>" for err in errors)
    error_block = (
        f"<section><h2>Decode Errors</h2><ul>{error_items}</ul></section>"
        if errors
        else ""
    )

    return f"""<!doctype html>
<html lang="en">
<head>
<meta charset="utf-8">
<title>{html.escape(title)}</title>
<style>
:root {{
  --label-w: 260px;
  --row-h: {base_row_h}px;
}}
body {{
  margin: 0;
  font-family: -apple-system, BlinkMacSystemFont, "Segoe UI", sans-serif;
  color: #1c2027;
  background: #eef1f5;
}}
header {{
  padding: 22px 28px 14px;
  border-bottom: 1px solid #d5dae3;
  background: #ffffff;
  position: sticky;
  top: 0;
  z-index: 10;
}}
h1 {{
  margin: 0 0 8px;
  font-size: 24px;
}}
.meta {{
  color: #5b6472;
  font-size: 14px;
  display: flex;
  flex-wrap: wrap;
  gap: 14px;
}}
.trace-shell {{
  display: grid;
  grid-template-columns: minmax(0, 1fr) 320px;
  gap: 18px;
  padding: 18px 20px 28px;
}}
.trace-panel {{
  background: #ffffff;
  border: 1px solid #dfe3ea;
  min-width: 0;
}}
.trace-scroll {{
  overflow: auto;
  max-height: calc(100vh - 160px);
  position: relative;
}}
.ruler {{
  display: flex;
  height: 54px;
  position: sticky;
  top: 0;
  z-index: 4;
  background: #fbfcfe;
  border-bottom: 1px solid #dfe3ea;
}}
.ruler-label {{
  width: var(--label-w);
  flex: 0 0 var(--label-w);
  position: sticky;
  left: 0;
  z-index: 5;
  background: #fbfcfe;
  border-right: 1px solid #dfe3ea;
  padding: 18px 14px;
  box-sizing: border-box;
  font-size: 12px;
  font-weight: 700;
  color: #384252;
}}
.ruler-track {{
  position: relative;
  flex: 0 0 {chart_w}px;
  height: 54px;
  background:
    linear-gradient(to right, rgba(67, 76, 92, .10) 1px, transparent 1px) 28px 0 / {grid_step}px 100%;
}}
.tick {{
  position: absolute;
  top: 0;
  bottom: 0;
  border-left: 1px solid #aeb7c4;
}}
.tick span {{
  position: absolute;
  top: 9px;
  left: 5px;
  font-size: 11px;
  color: #485366;
}}
.trace-row {{
  display: flex;
  min-height: var(--row-h);
  border-bottom: 1px solid #edf0f4;
}}
.trace-row.task {{
  background: #fbfbfd;
}}
.trace-row.object {{
  background: #ffffff;
}}
.lane-label {{
  width: var(--label-w);
  flex: 0 0 var(--label-w);
  position: sticky;
  left: 0;
  z-index: 3;
  box-sizing: border-box;
  padding: 11px 14px;
  border-right: 1px solid #dfe3ea;
  background: inherit;
  font-size: 12px;
  white-space: nowrap;
  overflow: hidden;
  text-overflow: ellipsis;
}}
.trace-row.task .lane-label {{
  font-weight: 700;
  color: #273243;
}}
.trace-row.object .lane-label {{
  color: #4f5a69;
}}
.track {{
  position: relative;
  height: var(--row-h);
  background:
    linear-gradient(to right, rgba(67, 76, 92, .08) 1px, transparent 1px) 28px 0 / {grid_step}px 100%;
}}
.event-block {{
  position: absolute;
  height: {event_h}px;
  border: 0;
  border-left: 4px solid var(--event-color);
  background: #f8fafc;
  box-shadow: inset 0 0 0 1px #cfd6e1;
  color: #18202c;
  font: 11px/26px ui-monospace, SFMono-Regular, Menlo, Consolas, monospace;
  text-align: left;
  padding: 0 7px 0 6px;
  overflow: hidden;
  white-space: nowrap;
  text-overflow: ellipsis;
  cursor: pointer;
}}
.event-block:hover,
.event-block:focus {{
  outline: 2px solid #1d73b7;
  z-index: 2;
}}
.event-block.task_prio {{
  border-radius: 12px;
}}
.drop-badge {{
  margin-left: 6px;
  color: #a02f25;
  font-weight: 700;
}}
.side-panel {{
  position: sticky;
  top: 94px;
  align-self: start;
  background: #ffffff;
  border: 1px solid #dfe3ea;
  padding: 14px;
}}
.side-panel h2 {{
  margin: 0 0 10px;
  font-size: 16px;
}}
.filter {{
  display: grid;
  gap: 6px;
  margin-bottom: 14px;
}}
.filter label {{
  font-size: 12px;
  font-weight: 700;
  color: #384252;
}}
.filter select {{
  width: 100%;
  min-height: 34px;
  border: 1px solid #cbd3df;
  background: #ffffff;
  color: #1c2027;
  padding: 6px 8px;
  font: 13px -apple-system, BlinkMacSystemFont, "Segoe UI", sans-serif;
}}
.detail {{
  min-height: 180px;
  white-space: pre-wrap;
  overflow-wrap: anywhere;
  font: 12px/1.5 ui-monospace, SFMono-Regular, Menlo, Consolas, monospace;
  background: #f7f8fa;
  border: 1px solid #e1e5ec;
  padding: 10px;
}}
.legend {{
  display: flex;
  flex-wrap: wrap;
  gap: 10px;
  margin-top: 12px;
  font-size: 12px;
  color: #4f5a69;
}}
.legend span {{
  display: inline-flex;
  align-items: center;
  gap: 5px;
}}
.legend i {{
  width: 12px;
  height: 12px;
  display: inline-block;
}}
.hidden {{
  display: none;
}}
section {{
  margin: 0 28px 28px;
}}
</style>
</head>
<body>
<header>
<h1>{html.escape(title)}</h1>
<div class="meta">
  <span>{len(events)} decoded events</span>
  <span>{object_count} object records</span>
  <span>{prio_count} priority records</span>
  <span>{overrun_count} overruns</span>
  <span>{lane_count} lanes</span>
  <span>ticks {min_tick}..{max_tick}</span>
  <span>{dropped_total} dropped before drain</span>
</div>
</header>
<main class="trace-shell">
<section class="trace-panel">
  <div class="trace-scroll">
    <div class="ruler">
      <div class="ruler-label">Time</div>
      <div class="ruler-track" style="width:{chart_w}px">{''.join(tick_marks)}</div>
    </div>
    {''.join(row_html)}
  </div>
</section>
<aside class="side-panel">
  <h2>Selection</h2>
  <div class="filter">
    <label for="objectFilter">Kernel object</label>
    <select id="objectFilter">{''.join(object_options)}</select>
  </div>
  <div id="detail" class="detail">Click an operation block.</div>
  <div class="legend">{legend}</div>
</aside>
</main>
{error_block}
<script>
const detail = document.getElementById('detail');
const objectFilter = document.getElementById('objectFilter');
const rows = Array.from(document.querySelectorAll('.trace-row'));
const blocks = Array.from(document.querySelectorAll('.event-block'));

function applyFilter() {{
  const selected = objectFilter.value;
  blocks.forEach((button) => {{
    const visible = selected === '__all__' || button.dataset.subject === selected;
    button.classList.toggle('hidden', !visible);
  }});
  rows.forEach((row) => {{
    const lane = row.dataset.lane;
    const hasVisibleBlock = Array.from(row.querySelectorAll('.event-block'))
      .some((button) => !button.classList.contains('hidden'));
    const visible = selected === '__all__' || lane === selected || hasVisibleBlock;
    row.classList.toggle('hidden', !visible);
  }});
}}

objectFilter.addEventListener('change', () => {{
  detail.textContent = 'Click an operation block.';
  applyFilter();
}});

blocks.forEach((button) => {{
  button.addEventListener('click', () => {{
    const value = JSON.parse(button.dataset.detail);
    detail.textContent = JSON.stringify(value, null, 2);
  }});
}});

applyFilter();
</script>
</body>
</html>
"""


def main() -> int:
    parser = argparse.ArgumentParser()
    parser.add_argument("log", type=Path, help="QEMU stdout log")
    parser.add_argument("-o", "--output", type=Path, default=Path("ktrace_timeline.html"))
    parser.add_argument("--jsonl", type=Path, help="optional decoded JSONL output")
    args = parser.parse_args()

    events, errors = load_events(args.log)

    if args.jsonl:
        with args.jsonl.open("w", encoding="utf-8") as handle:
            for event in events:
                handle.write(json.dumps(event.__dict__, sort_keys=True) + "\n")

    args.output.write_text(
        render_html(events, errors, f"RK0 Trace Timeline: {args.log.name}"),
        encoding="utf-8",
    )

    print(f"events={len(events)} lanes={len(lane_order(events))} errors={len(errors)}")
    print(f"wrote {args.output}")
    return 1 if errors else 0


if __name__ == "__main__":
    raise SystemExit(main())
