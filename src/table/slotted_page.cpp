#include "duck/table/slotted_page.hpp"
#include "duck/common/types.hpp"
#include "duck/config/sizes.hpp"
#include <algorithm>
#include <array>
#include <cstddef>
#include <cstdint>
#include <optional>
#include <print>
#include <span>
#include <vector>

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

    const Slot& slot = slots_[slot_num];
    if (slot.offset == INVALID_SLOT_OFFSET)
        return {};

    return data_.subspan(slot.offset, slot.length);
}

bool SlottedPage::try_update_in_place(RID rid, std::span<const std::byte> tuple_data) {
    if (rid.slot_num >= header_->slot_count)
        return false;

    Slot& slot = slots_[rid.slot_num];
    if (slot.offset == INVALID_SLOT_OFFSET)
        return false;

    std::uint16_t tuple_size{static_cast<std::uint16_t>(tuple_data.size())};
    if (tuple_size > slot.length)
        return false;

    auto dest{data_.subspan(slot.offset, tuple_size)};
    std::copy(tuple_data.begin(), tuple_data.end(), dest.begin());
    slot.length = tuple_size;

    return true;
}

bool SlottedPage::delete_tuple(std::uint16_t slot_num) {
    if (slot_num >= header_->slot_count)
        return false;

    if (slots_[slot_num].offset == INVALID_SLOT_OFFSET)
        return false;

    slots_[slot_num].offset = INVALID_SLOT_OFFSET;
    return true;
}

bool SlottedPage::is_compacted() const {
    if (header_->slot_count == 0)
        return true;

    std::uint16_t next_offset{slots_[0].offset};
    for (std::uint16_t i{0}; i < header_->slot_count; ++i) {
        if (slots_[i].offset == INVALID_SLOT_OFFSET)
            continue;

        if (next_offset != slots_[i].offset)
            return false;

        next_offset += slots_[i].length;
    }

    return next_offset == kPAGE_SIZE;
}

// can be optimized to copy only from offset which has deleted data till end
void SlottedPage::compact() {
    PageHeader compacted_header{*header_};

    std::vector<Slot> compacted_slots;
    compacted_slots.reserve(compacted_header.slot_count);

    std::array<std::byte, kPAGE_SIZE> compacted{};
    std::uint16_t offset{kPAGE_SIZE};

    for (std::uint16_t i{0}; i < header_->slot_count; ++i) {
        const Slot& slot{slots_[i]};
        if (slot.offset == INVALID_SLOT_OFFSET) {
            compacted_slots.push_back({INVALID_SLOT_OFFSET, 0});
            continue;
        }

        offset -= slot.length;
        std::span<std::byte> slot_data{data_.subspan(slot.offset, slot.length)};

        std::copy(slot_data.begin(), slot_data.end(), compacted.begin() + offset);
        compacted_slots.push_back({offset, slot.length});
    }

    compacted_header.free_space_offset = offset;

    // rewrite data
    std::fill(data_.begin(), data_.end(), std::byte{0});

    std::memcpy(data_.data(), &compacted_header, sizeof(PageHeader));
    std::memcpy(data_.data() + sizeof(PageHeader), compacted_slots.data(), compacted_slots.size() * sizeof(Slot));
    std::copy(compacted.begin() + offset, compacted.end(), data_.begin() + offset);
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