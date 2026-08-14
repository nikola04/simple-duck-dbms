/**
 * Copyright (c) 2026 Nikola Nedeljkovic
 * SPDX-License-Identifier: MIT
 */

#include "duck/buffer/pinned_page.hpp"
#include "duck/buffer/pool_manager.hpp"
#include "duck/storage/disk_manager.hpp"
#include <cstddef>
#include <exception>
#include <iostream>
#include <print>

int main() {
    try {
        duck::DiskManager disk_manager{"test.db"};
        duck::BufferPoolManager pool{disk_manager, 5};

        duck::PinnedPage page = duck::make_pinned(pool.new_page(), &pool);
        std::println("{}: {}", page.page_id(), static_cast<const void*>(page.data()));
        duck::PinnedPage page2 = duck::make_pinned(pool.new_page(), &pool);

        // std::println("{}: {}", page.page_id(), page.data());
        // std::println("{}: {}", page2.page_id(), page2.data());
    } catch (std::exception& e) {
        std::cout << "Exception: " << e.what() << "\n";
    }
}