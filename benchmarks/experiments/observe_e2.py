#!/usr/bin/env python3
"""Generate observation-only native copies; fail if insertion sites drift."""
import argparse
import hashlib
import json
from pathlib import Path

ROOT = Path(__file__).resolve().parents[2]


def replace_exact(source, old, new, count=1):
    if source.count(old) != count:
        raise RuntimeError(f"observer site changed: expected {count}: {old!r}")
    return source.replace(old, new)


def generate(destination):
    destination.mkdir(parents=True, exist_ok=True)
    changes = {
        "kmesg.c": [
            ("    *mesgPPtr = mesgPtr;\n", "    *mesgPPtr = mesgPtr;\n    E2ObserveGrant(mesgPtr, RK_gRunPtr);\n", 1),
            ("    *(allocatorPtr->mesgEndpointPtr->allocDestPtr) = mesgPtr;\n", "    *(allocatorPtr->mesgEndpointPtr->allocDestPtr) = mesgPtr;\n    E2ObserveGrant(mesgPtr, allocatorPtr);\n", 1),
            ("    kMesgSetOwner_(mesgPtr, NULL);\n", "    kMesgSetOwner_(mesgPtr, NULL);\n    E2ObserveRelease(mesgPtr);\n", 1),
            ("    RK_ERR const err = kMemPartitionFree(poolPtr, mesgPtr);\n", "    RK_ERR const err = kMemPartitionFree(poolPtr, mesgPtr);\n    if (err == RK_ERR_SUCCESS) { E2ObserveAvailable(mesgPtr); }\n", 1),
            ("    kMesgSetOwner_(mesgPtr, taskHandle);\n", "    kMesgSetOwner_(mesgPtr, taskHandle);\n    E2ObserveSend(mesgPtr, taskHandle);\n", 1),
            ("    *(receiverPtr->mesgEndpointPtr->waitDestPtr) = mesgPtr;\n", "    *(receiverPtr->mesgEndpointPtr->waitDestPtr) = mesgPtr;\n    E2ObserveDirect(mesgPtr, receiverPtr);\n", 1),
        ],
        "kmesgq.c": [
            ("    kMesgQueueCopy_(kobj, recvTaskPtr->mesgQueueRecvBufPtr, sendPtr);\n", "    kMesgQueueCopy_(kobj, recvTaskPtr->mesgQueueRecvBufPtr, sendPtr);\n    E2ObserveQueueSend(kobj, sendPtr, recvTaskPtr);\n", 1),
            ("    kRingBufWrite(&kobj->ringBuf, (ULONG const *)sendPtr);\n", "    kRingBufWrite(&kobj->ringBuf, (ULONG const *)sendPtr);\n    E2ObserveQueueSend(kobj, sendPtr, NULL);\n", 1),
            ("    kRingBufRead(&kobj->ringBuf, (ULONG *)recvPtr);\n", "    kRingBufRead(&kobj->ringBuf, (ULONG *)recvPtr);\n    E2ObserveQueueRecv(kobj, recvPtr);\n", 1),
        ],
        "ksch.c": [
            ("        kTraceRecordTaskPrio(tcbPtr, oldPrio, newPrio);\n", "        kTraceRecordTaskPrio(tcbPtr, oldPrio, newPrio);\n        E2ObservePriority(tcbPtr, oldPrio, (RK_BOOL)(waitQueuePtr != NULL));\n", 3),
        ],
    }
    manifest = {}
    for name, edits in changes.items():
        original = (ROOT / "core/src" / name).read_text()
        observed = original
        for old, new, count in edits:
            if name == "kmesgq.c" and "kRingBuf" in old:
                begin = "RK_ERR kMesgQueueSend(" if "Write" in old else "RK_ERR kMesgQueueRecv("
                end = "RK_ERR kMesgQueueRecv(" if "Write" in old else "RK_ERR kMesgQueuePeek("
                first = observed.index(begin)
                last = observed.index(end, first)
                observed = (observed[:first] + replace_exact(observed[first:last], old, new, count)
                            + observed[last:])
            else:
                observed = replace_exact(observed, old, new, count)
        observed = '#include "e2_observe.h"\n' + observed
        (destination / name).write_text(observed)
        manifest[name] = {
            "source_sha256": hashlib.sha256(original.encode()).hexdigest(),
            "observed_sha256": hashlib.sha256(observed.encode()).hexdigest(),
            "insertions": sum(edit[2] for edit in edits),
        }
    (destination / "observers.json").write_text(json.dumps(manifest, indent=2) + "\n")
    return manifest


if __name__ == "__main__":
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument("destination", type=Path)
    args = parser.parse_args()
    generate(args.destination)
