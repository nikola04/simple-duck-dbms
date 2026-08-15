# Simple Duck DBMS

Should be a simple multithreaded Database Managment system build in C++23

### Known limitations

- **Cross-table RID access is not validated.** `TableHeap::GetTuple`/`DeleteTuple` fetch a page directly via `RID.page_id` without checking that the page actually belongs to this heap's page chain. Validating this would require walking the chain (defeating the O(1) purpose of a RID) or adding a `table_id` tag to `PageHeader`. For now this is a trust boundary enforced by upper layers (Catalog/Executor), not by `TableHeap` itself — trading correctness guarantees for O(1) get/delete performance.
- **Orphaned page on concurrent insert race.** When two threads simultaneously find the last page in a chain full and both allocate a new page to link, only the "winning" thread's page gets linked via `set_next_page`. The loser's freshly allocated page is never linked to anything and is leaked (never reclaimed by `DiskManager`'s free-list). A full fix would call `disk_manager_.deallocate_page(...)` on the orphaned page in that branch.
- **No page compaction.** `SlottedPage::DeleteTuple` only marks a slot as empty (`length = 0`); the physical bytes of a deleted tuple are never reclaimed until the slot itself is reused by a same-or-smaller insert. Fragmented space in the middle of a page is never compacted, so a page can report "no space" for an insert even if the sum of free bytes would technically fit. Compaction (repacking live tuples, updating slot offsets in place, RIDs unaffected) is a natural extension but not implemented for v1.
- **Flush during eviction holds the global BPM latch.** `BufferPoolManager::swap_page` performs the dirty-page flush (disk write) while still holding the global `latch_`, serializing all other BPM operations during that I/O. The read of the incoming page is correctly done outside the lock; splitting the flush the same way (reserve-then-release-then-flush) is a known follow-up optimization.
- **DiskManager is POSIX-only.** Uses `pread`/`pwrite` directly; no Windows (`ReadFile`/`WriteFile` + `OVERLAPPED`) backend.
- **`reinterpret_cast` over raw page bytes (`PageHeader`, `Slot`) is technically in a strict-aliasing grey area** pre-C++23 `start_lifetime_as`. In practice this is the standard, universally-used approach for on-disk binary formats and works reliably on GCC/Clang for standard-layout structs, but it isn't formally guaranteed by the standard.

## License

This project is licensed under the [MIT License](./LICENSE).
