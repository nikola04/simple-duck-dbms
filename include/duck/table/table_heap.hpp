#pragma once

#include "duck/buffer/pool_manager.hpp"
#include "duck/common/iterator.hpp"
#include "duck/common/rid.hpp"
#include "duck/common/types.hpp"
#include <cstddef>
#include <optional>
#include <span>
#include <vector>

namespace duck {

class TableHeap {
public:
    TableHeap(PageID first_page_id, BufferPoolManager& bpm) : bpm_(bpm), first_page_id_(first_page_id) {
    }

    static TableHeap create(BufferPoolManager& bpm);

    std::optional<RID> insert_tuple(std::span<const std::byte> tuple_data);
    std::optional<std::vector<std::byte>> get_tuple(RID rid);
    bool delete_tuple(RID rid);

    Iterator begin();
    Iterator end();

private:
    BufferPoolManager& bpm_;
    PageID first_page_id_;
};

} // namespace duck