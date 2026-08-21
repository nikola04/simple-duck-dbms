#include "duck/table/table_heap.hpp"
#include "duck/buffer/page.hpp"
#include "duck/buffer/pinned_page.hpp"
#include "duck/common/rid.hpp"
#include "duck/common/types.hpp"
#include "duck/table/slotted_page.hpp"
#include <cstddef>
#include <cstdint>
#include <mutex>
#include <optional>
#include <ranges>
#include <shared_mutex>
#include <stdexcept>
#include <vector>

namespace duck {

TableHeap TableHeap::create(BufferPoolManager& bpm) {
    duck::PinnedPage page{duck::make_pinned(bpm.new_page(), &bpm)};
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
    if (tuple_data.size() > kPAGE_SIZE - sizeof(PageHeader) - sizeof(Slot)) {
        throw std::runtime_error("TableHeap::insert_tuple: tuple exceeds maximum page capacity");
    }

    PageID page_id{first_page_id_};
    bool created_new_page{false};
    while (Page * page{bpm_.fetch_page(page_id)}) {
        PinnedPage pinned{page, &bpm_};

        std::unique_lock<std::shared_mutex> lock{page->latch()};
        SlottedPage slotted{page->data()};

        if (auto rid{slotted.insert_tuple(tuple_data)}; rid.has_value()) {
            rid->page_id = page->page_id();
            pinned.mark_dirty();
            return rid;
        }

        if (!slotted.is_compacted()) {
            slotted.compact();
            if (auto rid{slotted.insert_tuple(tuple_data)}; rid.has_value()) {
                rid->page_id = page->page_id();
                pinned.mark_dirty();
                return rid;
            }
        }

        // For now this will be a trade-off for data consistency
        // If someone wants to access memory he will need to wait for unique lock while next page is created
        // Because thread B can start allocating new page while thread A released lock and started allocating it already
        if (PageID next_page_id{slotted.next_page()}; next_page_id == INVALID_PAGE_ID) {
            if (created_new_page) {
                throw std::runtime_error(
                    "TableHeap::insert_tuple: tuple failed to fit even on a freshly created page — "
                    "this indicates a bug in SlottedPage size accounting");
            }
            lock.unlock();

            Page* next_page = bpm_.new_page();
            created_new_page = true;
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

        std::span<const std::byte> view{slotted.get_tuple(rid.slot_num)};
        if (view.empty())
            return std::nullopt;

        std::vector<std::byte> tuple{std::from_range, view};
        return tuple;
    }

    return std::nullopt;
}

std::optional<RID> TableHeap::update_tuple(RID rid, std::span<const std::byte> tuple) {
    if (Page* page{bpm_.fetch_page(rid.page_id)}; page != nullptr) {
        PinnedPage pinned{page, &bpm_};
        std::unique_lock<std::shared_mutex> lock{page->latch()};

        SlottedPage slotted{page->data()};

        if (!slotted.has_slot(rid.slot_num))
            return std::nullopt;

        if (slotted.try_update_in_place(rid, tuple)) {
            pinned.mark_dirty();
            return rid;
        }
    } // locks released, page unpined

    delete_tuple(rid);
    return insert_tuple(tuple);
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

TableHeap::Scan TableHeap::scan() const {
    return {first_page_id_, &bpm_};
}

TableHeap::Scan::Scan(PageID first_page_id, BufferPoolManager* bpm)
    : bpm_(bpm), current_rid_(RID{.page_id = first_page_id, .slot_num = 0}) {
    current_page_ = bpm->fetch_page(first_page_id);
}

TableHeap::Scan::~Scan() {
    if (current_page_ != nullptr)
        bpm_->unpin_page(current_page_->page_id(), false);
}

std::optional<std::pair<RID, std::vector<std::byte>>> TableHeap::Scan::next() {
    while (current_page_ != nullptr) {
        std::shared_lock<std::shared_mutex> lock{current_page_->latch()};
        SlottedPage slotted{current_page_->data()};

        // try to find next occupied because current became deleted:
        if (!slotted.has_slot(current_rid_.slot_num)) {
            std::optional<std::uint16_t> next_slot{slotted.next_occupied_slot(current_rid_.slot_num)};
            if (next_slot.has_value()) {
                current_rid_.slot_num = next_slot.value();
            }
        }

        // if we didnt find next occupied on that page we try to switch to next page:
        if (!slotted.has_slot(current_rid_.slot_num)) {
            lock.unlock();

            if (next_page())
                continue; // -> repeat same logic for next page
            else
                return std::nullopt;
        }

        std::span<const std::byte> view{slotted.get_tuple(current_rid_.slot_num)};
        std::vector<std::byte> tuple{std::from_range, view};
        RID rid = current_rid_;

        current_rid_.slot_num++;

        return std::pair{rid, tuple};
    }

    return std::nullopt;
}

bool TableHeap::Scan::next_page() {

    if (current_page_ == nullptr)
        return false;

    // release current page
    PageID next_page_id;
    {
        std::shared_lock<std::shared_mutex> lock{current_page_->latch()};
        SlottedPage slotted{current_page_->data()};

        if (!slotted.has_next_page())
            return false;

        next_page_id = slotted.next_page();
    }
    bpm_->unpin_page(current_page_->page_id(), false);

    // fetch next one
    if (Page* next_page = bpm_->fetch_page(next_page_id); next_page != nullptr) {
        current_page_ = next_page;

        current_rid_.page_id = current_page_->page_id();
        current_rid_.slot_num = 0;

        return true;
    }

    return false;
}

} // namespace duck