#pragma once

#include "duck/buffer/page.hpp"
#include "duck/buffer/pool_manager.hpp"
#include "duck/storage/disk_manager.hpp"
#include <cstddef>

namespace duck {

class PinnedPage {
public:
    PinnedPage(Page* page, BufferPoolManager* bpm) : page_(page), bpm_(bpm) {
    }
    ~PinnedPage() {
        if (valid())
            bpm_->unpin_page(page_->page_id(), dirty_);
    }
    PinnedPage(const PinnedPage&) = delete;
    PinnedPage& operator=(const PinnedPage&) = delete;

    PinnedPage(PinnedPage&& other) noexcept : page_(other.page_), bpm_(other.bpm_) {
        other.page_ = nullptr;
    }

    PinnedPage& operator=(PinnedPage&& other) {
        if (this != &other) {
            if (valid())
                bpm_->unpin_page(page_->page_id(), dirty_);
            bpm_ = other.bpm_;
            page_ = other.page_;
            dirty_ = other.dirty_;
            other.page_ = nullptr;
        }
        return *this;
    }

    bool valid() const {
        return page_ != nullptr;
    }
    void mark_dirty() {
        dirty_ = true;
    }

    PageID page_id() const {
        if (!valid())
            return INVALID_PAGE_ID;
        return page_->page_id();
    }

    std::span<std::byte> data() const {
        if (!valid())
            return std::span<std::byte>{};
        return page_->data();
    }

private:
    Page* page_;
    BufferPoolManager* bpm_;

    bool dirty_{false};
};

PinnedPage make_pinned(Page* page, BufferPoolManager* bpm);

} // namespace duck