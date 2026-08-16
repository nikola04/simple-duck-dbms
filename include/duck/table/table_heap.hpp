#pragma once

#include "duck/buffer/pool_manager.hpp"
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

    class Scan;

    Scan scan();

private:
    BufferPoolManager& bpm_;
    PageID first_page_id_;
};

class TableHeap::Scan {
public:
    Scan(PageID first_page_id, BufferPoolManager* bpm);
    ~Scan();

    // should not allowed copy because of pinned page
    Scan(const Scan&) = delete;
    Scan& operator=(const Scan&) = delete;

    bool operator==(const Scan& other) {
        return current_rid_ == other.current_rid_;
    }

    std::optional<std::pair<RID, std::vector<std::byte>>> next();

private:
    BufferPoolManager* bpm_{};
    RID current_rid_{};
    Page* current_page_{nullptr};

    bool next_page();
};

} // namespace duck