# Simple Duck DBMS

A simple multithreaded Database Managment system build in C++23

## Architecture

Layered, bottom-up:

- **`duck_storage`** — `DiskManager`: raw page I/O (`pread`/`pwrite`), page allocation/deallocation with free-list reuse.
- **`duck_buffer`** — `BufferPoolManager`: fixed-size RAM cache over disk pages, LRU-based eviction, per-page latches (fine-grained locking, independent of the global buffer-pool latch), `PinnedPage` RAII wrapper.
- **`duck_tuple`** — `SlottedPage` (on-disk page layout for variable-length tuples), `Value`/`Schema`/`Column`/`Tuple` (typed row representation with serialization to/from raw bytes, NULL bitmap, fixed vs. variable-length column handling).
- **`duck_table`** — `TableHeap` (multi-page tuple storage with page-chain traversal), `Table` (schema-aware wrapper over `TableHeap`), cursor-style `Scan` for sequential iteration.
- **`duck_catalog`** — `Catalog`: persistent table registry. Bootstraps itself as an ordinary table (via `TableHeap`) rooted at a fixed, well-known page id, storing `{table_name, first_page_id, schema_bytes}` rows for every user-created table. On startup, scans its own page chain to reconstruct in-memory `Table` handles.

Each layer only depends on the one below it; concurrency guarantees (thread-safety, no data races) are verified independently at each layer under ThreadSanitizer.

### Known limitations

- **Cross-table RID access is not validated.** `TableHeap::GetTuple`/`DeleteTuple` fetch a page directly via `RID.page_id` without checking that the page actually belongs to this heap's page chain. Validating this would require walking the chain (defeating the O(1) purpose of a RID) or adding a `table_id` tag to `PageHeader`. For now this is a trust boundary enforced by upper layers (Catalog/Executor), not by `TableHeap` itself — trading correctness guarantees for O(1) get/delete performance.
- **Orphaned page on concurrent insert race.** When two threads simultaneously find the last page in a chain full and both allocate a new page to link, only the "winning" thread's page gets linked via `set_next_page`. The loser's freshly allocated page is never linked to anything and is leaked (never reclaimed by `DiskManager`'s free-list). A full fix would call `disk_manager_.deallocate_page(...)` on the orphaned page in that branch.
- **Flush during eviction holds the global BPM latch.** `BufferPoolManager::swap_page` performs the dirty-page flush (disk write) while still holding the global `latch_`, serializing all other BPM operations during that I/O. The read of the incoming page is correctly done outside the lock; splitting the flush the same way (reserve-then-release-then-flush) is a known follow-up optimization.
- **No length validation on `Value` construction.** `Value::of(std::string)` accepts strings of any length regardless of the target column's `CHAR`/`VARCHAR` constraint. Validation happens later, at `Tuple::Serialize()` time (which has access to the `Schema`), not at `Value` construction — by design, since a `Value` can exist independently of any target column.
- **DiskManager is POSIX-only.** Uses `pread`/`pwrite` directly; no Windows (`ReadFile`/`WriteFile` + `OVERLAPPED`) backend.
- **`reinterpret_cast` over raw page bytes (`PageHeader`, `Slot`) is technically in a strict-aliasing grey area** pre-C++23 `start_lifetime_as`. In practice this is the standard, universally-used approach for on-disk binary formats and works reliably on GCC/Clang for standard-layout structs, but it isn't formally guaranteed by the standard.

## License

This project is licensed under the [MIT License](./LICENSE).
