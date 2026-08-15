#include "duck/table/table_heap.hpp"
#include "duck/buffer/page.hpp"
#include "duck/buffer/pinned_page.hpp"
#include "duck/common/types.hpp"
#include "duck/tuple/slotted_page.hpp"
#include <cstddef>
#include <mutex>
#include <optional>
#include <ranges>
#include <shared_mutex>
#include <stdexcept>
#include <vector>

namespace duck {

TableHeap TableHeap::create(BufferPoolManager& bpm) {
    duck::PinnedPage page = duck::make_pinned(bpm.new_page(), &bpm);
    if (!page.valid())
        throw std::runtime_error("TableHeap: failed to create next page");

    std::lock_guard<std::shared_mutex> lock{page.page()->latch()};

    SlottedPage slotted{page.data()};
    slotted.init();
    page.mark_dirty();

    return TableHeap{page.page_id(), bpm};
}

// For now when we need new page we create it unpin it then pin it again in next iteration because of code readability
// Can be optimized but for now is acceptable
std::optional<RID> TableHeap::insert_tuple(std::span<const std::byte> tuple_data) {
    PageID page_id{first_page_id_};
    while (Page * page{bpm_.fetch_page(page_id)}) {
        PinnedPage pinned{page, &bpm_};

        std::unique_lock<std::shared_mutex> lock{page->latch()};
        SlottedPage slotted{page->data()};

        if (auto rid{slotted.insert_tuple(tuple_data)}; rid.has_value()) {
            rid->page_id = page->page_id();
            pinned.mark_dirty();
            return rid;
        }

        // maybe try to compact here? not implemented for now
        // slotted.compact();

        // For now this will be a trade-off for data consistency
        // If someone wants to access memory he will need to wait for unique lock while next page is created
        // Because thread B can start allocating new page while thread A released lock and started allocating it already
        if (PageID next_page_id{slotted.next_page()}; next_page_id == INVALID_PAGE_ID) {
            lock.unlock();

            Page* next_page = bpm_.new_page();
            if (next_page == nullptr)
                throw std::runtime_error("TableHeap: failed to create next page");

            {
                PinnedPage pinned_new{next_page, &bpm_};
                std::unique_lock<std::shared_mutex> next_lock{next_page->latch()};
                SlottedPage s{next_page->data()};
                s.init();
                pinned_new.mark_dirty();
            }

            lock.lock();

            // Re-check: did another thread link a next page while we didn't hold the lock?
            if (slotted.next_page() == INVALID_PAGE_ID) {
                slotted.set_next_page(next_page->page_id());
                pinned.mark_dirty();
            }
            // else: someone else already linked a page. Our new page is now orphaned —
            // for v1, we simply leak it (acceptable tradeoff, documented as known limitation).
            // A fuller fix would return it to disk_manager_'s free list via deallocate_page.
        }

        page_id = slotted.next_page();
    }

    return std::nullopt;
}

std::optional<std::vector<std::byte>> TableHeap::get_tuple(RID rid) {
    if (Page* page{bpm_.fetch_page(rid.page_id)}; page != nullptr) {
        PinnedPage pinned{page, &bpm_};
        std::shared_lock<std::shared_mutex> lock{page->latch()};

        SlottedPage slotted{page->data()};

        std::span<std::byte> view{slotted.get_tuple(rid.slot_num)};
        if (view.empty())
            return std::nullopt;

        std::vector<std::byte> tuple(std::from_range, view);
        return tuple;
    }

    return std::nullopt;
}

bool TableHeap::delete_tuple(RID rid) {
    if (Page* page{bpm_.fetch_page(rid.page_id)}; page != nullptr) {
        PinnedPage pinned{page, &bpm_};
        std::unique_lock<std::shared_mutex> lock{page->latch()};

        SlottedPage slotted{page->data()};
        bool deleted{slotted.delete_tuple(rid.slot_num)};

        if (deleted)
            pinned.mark_dirty();

        return deleted;
    }

    return false;
}

} // namespace duck