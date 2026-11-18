# Architecture and model assumptions

This document describes the **implemented** SSD GC Simulator model. It is an
educational, in-memory **page-mapped FTL**, not physical NAND simulation.

## Data and control flow

```text
apps/ssd_sim.cpp (CLI parses configuration)
   |
   +--> generateWorkload(WorkloadConfig) --> vector<LogicalPageId>
   |                                           |
   +--> FTL(policy, lambda)                     v
   |                                       runBenchmark(FTL&, workload)
   |                                           |
   |                                           v
   |                                     FTL::writeManaged(lpn)
   |                                           |
   |                          +----------------+----------------+
   |                          |                                 |
   |                          v                                 v
   |                     FTL mapping                       GC selection
   |                     LPN -> PPA                        and migration
   |                          |                                 |
   |                          +---------------+-----------------+
   |                                          |
   |                                          v
   |                                  Flash -> Block -> Page
   |
   +--> CLI summary / optional 26-column CSV
```

The core library is `ssd_core` (six implementation files in `src/`). The CLI
in `apps/ssd_sim.cpp` is a separate executable. Research executables in
`experiments/` are optional and are **not** the CLI's stable interface.

## Storage model and invariants

- `Page` has `Free`, `Valid`, or `Invalid` state and an optional `LogicalPageId`.
- `Block` has fixed capacity. Pages are programmed **sequentially** into unused
  positions. `Valid` -> `Invalid` denotes superseded data; an invalid page
  cannot be reprogrammed until the entire block is erased.
- A `Block::erase()` operation fails if the block still has any valid pages.
  Successful erase changes all pages to `Free` and increments its erase count.
- `Flash` owns blocks, assigns `PhysicalPageAddress { block, page }`, and
  maintains a global physical-program counter. A block receives a **closed
  epoch** when it becomes full; erase clears that block's epoch.
- `FTL` maps each live LPN to exactly one valid PPA. Updating an existing LPN
  writes a new physical page then invalidates the old one. A page's LPN should
  agree with the reverse mapping used during GC.

For every block, the following accounting identity must hold:

```text
freeCount + validCount + invalidCount == pagesPerBlock
```

For an FTL created with `B` blocks and `P` pages per block,
`logicalCapacity() == (B - 1) * P`. This is a **logical capacity bound**, not a
fully reserved physical block allocator. The managed write path is intended
for normal simulations; low-level `FTL::write()` bypasses automatic GC and the
managed logical-capacity check.

## Host writes and garbage collection

1. `runBenchmark()` iterates over a generated LPN write sequence and calls
   `FTL::writeManaged(lpn)` for each operation.
2. `writeManaged()` rejects *new* distinct LPNs beyond logical capacity. When
   free physical pages are at or below one block's capacity, it **attempts**
   one GC cycle before programming. GC may have no eligible victim.
3. The host write programs a free physical page. Overwrites invalidate the
   previous PPA; the LPN mapping is updated to the newly programmed PPA.
4. `garbageCollectOne()` selects one eligible victim, migrates its valid pages
   to free physical pages outside the victim, then erases it and records one
   `GcEvent`. If no eligible victim exists, it returns `false`.

### GC victim eligibility

The candidate block must be **full** (`freeCount == 0`), contain at least one
`Invalid` page, and have enough free physical pages **outside that block** to
relocate all of its valid pages. This avoids selecting a block that cannot be
migrated using the current free-space pool.

### Victim selection policies

| Policy | Selection rule among eligible blocks |
| --- | --- |
| `Greedy` | Most invalid pages (equivalently, fewest valid pages among equal-size full blocks). |
| `FIFO` | Earliest closed epoch (oldest filled eligible block). |
| `WearAwareGreedy` | Minimum `validCount + lambda * eraseCount`, with nonnegative lambda. |

Candidates are visited by ascending block index. Exact score ties retain the
first candidate, so smaller block index breaks ties. Increasing `lambda`
penalizes blocks that have already been erased more frequently, but may
increase migration work / WAF. This is a **heuristic**, not a lifetime model.

`GcEvent` records the selected victim, pre-migration valid/invalid page counts,
free pages outside the victim, number migrated, GC cycle count and closed epoch.

## Workload generation and repeatability

- **Sequential:** LPNs cycle through `[0, logical_page_count)` in order.
- **Random:** uniform LPN selection in the configured range.
- **HotCold:** a Bernoulli choice between the first `hot_set_size` LPNs and the
  remaining cold range, using `hot_probability` for choosing the hot range.

Random and HotCold use `std::mt19937_64` and standard-library distributions.
The seed is repeatable within a fixed library/toolchain; it is **not** a
portable byte-for-byte trace guarantee across implementations.

## Code entry points

| Responsibility | Public interface |
| --- | --- |
| Page / address model | `ssd::Page`, `ssd::PhysicalPageAddress` |
| Block operations | `ssd::Block` |
| Flash allocation / erase | `ssd::Flash` |
| Managed logical writes and GC | `ssd::FTL::writeManaged`, `garbageCollectOne` |
| GC policy | `ssd::GcPolicy` |
| Workload creation | `ssd::generateWorkload` |
| Run measurement | `ssd::runBenchmark` -> `ssd::BenchmarkResult` |

`Block` and `Flash` expose read-only inspection and explicit low-level
operations primarily for internal composition and tests. Public headers are
currently **pre-v1.0**, so API stability has not yet been promised.

## Limitations

No page payloads, NAND timing, read/trim commands, channel/die parallelism,
ECC, wear-out failures, crash consistency, or power-loss recovery are modeled.
There is no SSD latency/throughput prediction. The one-block logical capacity
margin, selection eligibility checks, and free-space behavior are simplified
educational rules, not a production flash controller.

For metric interpretation, see [METRICS.md](METRICS.md). For reproducing the
benchmark, see the root [README](../README.md) and [scripts/README.md](../scripts/README.md).
