# SSD GC Simulator design

## System model

A deterministic, metadata-only **page-mapped FTL** with a set of erase blocks.
A physical page has one of three states: Free, Valid or Invalid, plus an
optional logical page identifier. Logical updates program a new physical page,
then invalidate the old mapping.

```
Workload (vector of LogicalPageId)
          |
          v
      runBenchmark
          |
          v
     FTL::writeManaged  ---> mapping (LPN -> physical block/page)
          |                       |
          |            FTL::selectVictimBlock
          v                       |
         Flash <----------------- GC migrate / erase
          |
          v
      Block -> Page
```

## Core components

- `Block` models sequential, out-of-place page programming, invalidation,
  and block erase (guarded against erasing valid pages).
- `Flash` owns blocks, manages physical allocation and closed-block epochs.
- `FTL` owns the mapping table, orchestrates managed writes and garbage
  collection, and records GC events and write metrics.
- `generateWorkload()` creates deterministic Sequential, Random and HotCold
  logical writes for experiments.
- `runBenchmark()` executes those writes and measures delta metrics.

## GC victim eligibility and policy

A closed block with at least one Invalid page is eligible when its Valid pages
can be migrated into the currently free pages **outside** that block.

- **Greedy:** maximize Invalid page count (equivalent to minimize Valid pages
  among fully programmed blocks of equal capacity).
- **FIFO:** choose the oldest closed epoch.
- **Wear-aware greedy:** minimize `validCount + lambda * eraseCount`.

The implementation traverses blocks in increasing index order, so ties are
resolved by the smallest block index.

## Metrics

- `host_writes`: successfully completed host page writes.
- `gc_page_writes`: physical page programs caused by migration.
- `physical_programs`: all successful physical page programs.
- `WAF = physical_programs / host_writes`, when host_writes > 0.
- `gc_cycles`: completed GC erases.
- `erase_cv`: standard deviation / mean of per-block erase counts.

## Deliberate limitations

- There is no NAND timing, data payload, read/trim operation, parallelism,
  controller scheduling, ECC or actual wear-out model.
- `Block::erase()` refuses to erase valid pages as an educational invariant.
- The simulator has a simplified free-space and capacity policy; it does
  not implement a production SSD allocator or true device overprovisioning.
- `logical_page_count` configures workload LPN range, not instantaneous
  valid-page occupancy.
- Experiments are exploratory and are not proof of SSD lifetime or latency.
