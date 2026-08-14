#pragma once

#include "duck/config/sizes.hpp"
#include "duck/storage/disk_manager.hpp"
#include <cstdint>
#include <shared_mutex>

namespace duck {

class Page {
public:
    Page() = default;
    Page(const Page& page) = delete;
    Page& operator=(const Page&) = delete;

    char* data() {
        return data_;
    }
    const char* data() const {
        return data_;
    }

    PageID page_id() const {
        return page_id_;
    }

    bool is_dirty() const {
        return is_dirty_;
    }
    void set_dirty() {
        is_dirty_ = true;
    }

    std::uint32_t pin_count() const {
        return pin_count_;
    }
    void inc_pin_count() {
        pin_count_++;
    }
    void dec_pin_count() {
        pin_count_--;
    }

    std::shared_mutex& latch() {
        return latch_;
    }

    void reset_memory(PageID page_id);

private:
    PageID page_id_{INVALID_PAGE_ID};
    char data_[kPAGE_SIZE]{0};

    std::uint32_t pin_count_{0};
    bool is_dirty_{false};

    std::shared_mutex latch_{};
};

} // namespace duck