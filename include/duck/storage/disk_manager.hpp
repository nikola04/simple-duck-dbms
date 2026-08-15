#pragma once

#include "duck/common/types.hpp"

#include <atomic>
#include <cstddef>
#include <mutex>
#include <span>
#include <string>
#include <vector>

namespace duck {

class DiskManager {
public:
    explicit DiskManager(std::string path);
    ~DiskManager();

    void read_page(PageID page_id, std::span<std::byte> buffer);
    void write_page(PageID page_id, std::span<const std::byte> buffer);

    PageID allocate_page();
    void deallocate_page(PageID page_id);

    size_t capacity() const {
        return capacity_;
    }

    void flush_all();

private:
    const std::string path_;
    const int fd_;

    std::atomic<PageID> capacity_{0};

    std::vector<PageID> free_list_{};
    std::mutex free_list_mutex{};

    off_t get_size() const;
};

} // namespace duck