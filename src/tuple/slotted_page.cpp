#include "duck/tuple/slotted_page.hpp"
#include "duck/common/types.hpp"
#include <algorithm>
#include <cstddef>
#include <cstdint>
#include <optional>

namespace duck {

SlottedPage::SlottedPage(std::span<std::byte> data)
    : data_(data), header_(reinterpret_cast<PageHeader*>(data_.data())),
      slots_(reinterpret_cast<Slot*>(data_.data() + sizeof(PageHeader))) {
}

void SlottedPage::init() {
    header_->next_page_id = INVALID_PAGE_ID;
    header_->free_space_offset = static_cast<uint16_t>(data_.size());
    header_->slot_count = 0;
}

std::int32_t SlottedPage::free_space_bytes() const {
    std::uint32_t slots_used = sizeof(PageHeader) + (header_->slot_count * sizeof(Slot));
    return static_cast<int32_t>(header_->free_space_offset) - static_cast<int32_t>(slots_used);
}

std::optional<std::uint16_t> SlottedPage::find_empty_slot() const {
    for (uint16_t i{0}; i < header_->slot_count; i++) {
        Slot& slot{slots_[i]};
        if (slot.offset == INVALID_SLOT_OFFSET)
            return {i};
    }

    return std::nullopt;
}

std::uint16_t SlottedPage::allocate_slot() {
    uint16_t slot_idx = header_->slot_count++;
    slots_[slot_idx].offset = INVALID_SLOT_OFFSET;
    return slot_idx;
}

std::optional<RID> SlottedPage::insert_tuple(std::span<const std::byte> tuple_data) {
    int16_t available_space = free_space_bytes();
    uint16_t tuple_size = static_cast<uint16_t>(tuple_data.size_bytes());

    if (available_space < tuple_size)
        return std::nullopt;

    uint16_t slot_idx;

    if (auto slot = find_empty_slot(); slot.has_value())
        slot_idx = slot.value();
    else {
        if (available_space - tuple_size < static_cast<int16_t>(sizeof(Slot)))
            return std::nullopt;

        slot_idx = allocate_slot();
    }

    Slot* slot = &slots_[slot_idx];
    slot->length = tuple_size;
    slot->offset = header_->free_space_offset - slot->length;
    header_->free_space_offset = slot->offset;

    auto dest = data_.subspan(slot->offset, slot->length);
    std::copy(tuple_data.begin(), tuple_data.end(), dest.begin());

    return {{INVALID_PAGE_ID, slot_idx}};
}

std::span<std::byte> SlottedPage::get_tuple(std::uint16_t slot_num) {
    if (slot_num >= header_->slot_count)
        return {};

    Slot& slot = slots_[slot_num];
    if (slot.offset == INVALID_SLOT_OFFSET)
        return {};

    return data_.subspan(slot.offset, slot.length);
}

bool SlottedPage::delete_tuple(std::uint16_t slot_num) {
    if (slot_num >= header_->slot_count)
        return false;

    if (slots_[slot_num].offset == INVALID_SLOT_OFFSET)
        return false;

    slots_[slot_num].offset = INVALID_SLOT_OFFSET;
    return true;
}

void SlottedPage::compact() {
    // compact tuples
}

void SlottedPage::set_next_page(PageID page_id) {
    header_->next_page_id = page_id;
}

PageID SlottedPage::next_page() const {
    return header_->next_page_id;
}

bool SlottedPage::has_slot(std::uint16_t slot_num) {
    return slot_num < header_->slot_count && slots_[slot_num].offset != INVALID_SLOT_OFFSET;
}

std::optional<std::uint16_t> SlottedPage::next_occupied_slot(std::uint16_t slot_num) {
    ++slot_num;
    for (; slot_num < header_->slot_count; ++slot_num) {
        if (slots_[slot_num].offset != INVALID_SLOT_OFFSET)
            return slot_num;
    }

    return std::nullopt;
}

bool SlottedPage::has_next_page() {
    return header_->next_page_id != INVALID_PAGE_ID;
}

} // namespace duck