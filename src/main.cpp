/**
 * Copyright (c) 2026 Nikola Nedeljkovic
 * SPDX-License-Identifier: MIT
 */

#include "duck/config/sizes.hpp"
#include "duck/storage/disk_manager.hpp"
#include <cstddef>
#include <exception>
#include <iostream>
#include <print>

int main() {
    try {
        duck::DiskManager disk_manager{"test.db"};
        size_t page_id{disk_manager.allocate_page()};

        disk_manager.write_page(page_id, "TEST2 AAA");

        char buffer[duck::kPAGE_SIZE];
        disk_manager.read_page(page_id, buffer);

        std::println("{}, {}: {}", page_id, disk_manager.capacity(), buffer);

        for (duck::PageID i{0}; i < disk_manager.capacity(); i++) {
            char buffer[duck::kPAGE_SIZE];
            disk_manager.read_page(i, buffer);
            std::println("{}: {}", i, buffer);
        }
    } catch (std::exception& e) {
        std::cout << "Exception: " << e.what() << "\n";
    }
}