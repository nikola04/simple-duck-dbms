#pragma once

#include "../common/rid.hpp"
#include <cstddef>
#include <cstdint>
#include <optional>
#include <span>

namespace duck {

struct [[gnu::packed]] PageHeader {
    PageID next_page_id;
    std::uint16_t slot_count;
    std::uint16_t free_space_offset;
};

struct [[gnu::packed]] Slot {
    std::uint16_t offset;
    std::uint16_t length;
};

class SlottedPage {
public:
    explicit SlottedPage(std::span<std::byte> data);

    void init();

    std::optional<RID> insert_tuple(std::span<const std::byte> tuple_data);
    std::span<std::byte> get_tuple(std::uint16_t slot_num);
    bool delete_tuple(std::uint16_t slot_num);

    std::int32_t free_space_bytes() const;
    void compact();

private:
    std::span<std::byte> data_;

    PageHeader* header_;
    Slot* slots_;

    std::optional<std::uint16_t> find_empty_slot() const;
    std::uint16_t allocate_slot();
};

} // namespace duck

// slot_count(16) free_space_offset(16) [slot1] [slot2] [slot...] ... FREE_SPACE ... [tuple...] [tuple2] [tuple1]