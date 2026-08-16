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
        duck::BufferPoolManager pool{disk_manager, 5};

        // duck::TableHeap heap{duck::TableHeap::create(pool)};
        duck::TableHeap heap{0, pool};

        // heap.delete_tuple({0, 5});

        // std::array<std::byte, 1024> barr;
        // barr.fill(std::byte{0x1f});
        // std::span<std::byte> value{barr};
        // heap.insert_tuple(value);

        duck::TableHeap::Scan scan = heap.scan();
        while (auto entry = scan.next()) {
            std::println("RID: {}/{}, size: {}", entry->first.page_id, entry->first.slot_num, entry->second.size());
        }

    } catch (std::exception& e) {
        std::cout << "Exception: " << e.what() << "\n";
    }
}