#pragma once

#include "duck/common/types.hpp"
#include "duck/config/sizes.hpp"
#include <array>
#include <cstddef>
#include <cstdint>
#include <shared_mutex>
#include <span>

namespace duck {

class Page {
public:
    Page() = default;
    Page(const Page& page) = delete;
    Page& operator=(const Page&) = delete;

    std::span<std::byte> data() {
        return data_;
    }
    std::span<const std::byte> data() const {
        return data_;
    }

    PageID page_id() const {
        return page_id_;
    }

    bool is_dirty() const {
        return is_dirty_;
    }
    void set_dirty(bool dirty = true) {
        is_dirty_ = dirty;
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

    void reset_memory() {
        reset_memory(INVALID_PAGE_ID);
    }

private:
    PageID page_id_{INVALID_PAGE_ID};
    std::array<std::byte, kPAGE_SIZE> data_{std::byte{0}};

    std::uint32_t pin_count_{0};
    bool is_dirty_{false};

    std::shared_mutex latch_{};
};

} // namespace duck