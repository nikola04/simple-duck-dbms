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
        duck::BufferPoolManager pool{disk_manager, 2};

        duck::PinnedPage page = duck::make_pinned(pool.fetch_page(0), &pool);
        std::println("{}: {}", page.page_id(), static_cast<void*>(page.data()));
        {
            duck::PinnedPage page2 = duck::make_pinned(pool.new_page(), &pool);
            std::println("{}: {}", page2.page_id(), static_cast<void*>(page2.data()));
        }
        duck::PinnedPage page3 = duck::make_pinned(pool.new_page(), &pool);
        std::println("{}: {}", page3.page_id(), static_cast<void*>(page3.data()));
        page.mark_dirty();

    } catch (std::exception& e) {
        std::cout << "Exception: " << e.what() << "\n";
    }
}