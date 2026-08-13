#pragma once

#include <atomic>
#include <cstddef>
#include <mutex>
#include <string>
#include <vector>

namespace duck {

class DiskManager {
public:
    explicit DiskManager(std::string path);
    ~DiskManager();

    void read_page(size_t page_id, char* buffer);
    void write_page(size_t page_id, const char* buffer);

    size_t allocate_page();
    void deallocate_page(size_t page_id);

    size_t capacity() const {
        return capacity_;
    }

private:
    const std::string path_;
    const int fd_;

    std::atomic<size_t> capacity_{0};

    std::vector<size_t> free_list_{};
    std::mutex free_list_mutex{};

    off_t get_size() const;
};

} // namespace duck