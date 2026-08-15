/**
 * Copyright (c) 2026 Nikola Nedeljkovic
 * SPDX-License-Identifier: MIT
 */

#include "duck/buffer/pool_manager.hpp"
#include "duck/storage/disk_manager.hpp"
#include "duck/table/table_heap.hpp"
#include <cstddef>
#include <exception>
#include <iostream>
#include <print>

int main() {
    try {
        duck::DiskManager disk_manager{"test.db"};
        duck::BufferPoolManager pool{disk_manager, 2};

        duck::TableHeap heap{0, pool};

        heap.delete_tuple({0, 0});

        std::array<std::byte, 18> barr;
        barr.fill(std::byte{0x1f});
        std::span<std::byte> value{barr};

        auto rid = heap.insert_tuple(value);
        std::println("inserted to page/slot: {} {}", rid->page_id, rid->slot_num);

    } catch (std::exception& e) {
        std::cout << "Exception: " << e.what() << "\n";
    }
}