#include "duck/storage/disk_manager.hpp"
#include "duck/config/sizes.hpp"

#include <cstring>
#include <fcntl.h>
#include <mutex>
#include <stdexcept>
#include <sys/stat.h>
#include <unistd.h>

namespace duck {

DiskManager::DiskManager(std::string path)
    : path_{std::move(path)}, fd_{open(path_.c_str(), O_RDWR | O_CREAT, S_IRUSR | S_IWUSR)} {

    if (fd_ == -1) {
        throw std::invalid_argument("Disk file failed to open: " + path_);
    }

    if (off_t size{get_size()}; size > 0) {
        capacity_ = size / kPAGE_SIZE;
    }
}

DiskManager::~DiskManager() {
    if (fd_ != -1)
        close(fd_);
}

PageID DiskManager::allocate_page() {
    if (std::lock_guard lock{free_list_mutex}; !free_list_.empty()) {
        PageID page_id{free_list_.back()};
        free_list_.pop_back();
        return page_id;
    }

    return capacity_.fetch_add(1);
}

void DiskManager::deallocate_page(PageID page_id) {
    std::lock_guard lock{free_list_mutex};
    free_list_.push_back(page_id);
}

void DiskManager::write_page(PageID page_id, const char* buffer) {
    if (page_id >= capacity_)
        throw std::runtime_error("DiskManager: trying to write into not allocated page: " + std::to_string(page_id));

    size_t offset{page_id * kPAGE_SIZE};
    ssize_t bytes_written{pwrite(fd_, buffer, kPAGE_SIZE, offset)};

    if (bytes_written != static_cast<ssize_t>(kPAGE_SIZE))
        throw std::runtime_error("DiskManager: incomplete write for page: " + std::to_string(page_id));
}

void DiskManager::read_page(PageID page_id, char* buffer) {
    if (page_id >= capacity_)
        throw std::runtime_error("DiskManager: trying to read not allocated page: " + std::to_string(page_id));

    size_t offset{page_id * kPAGE_SIZE};
    ssize_t bytes_read{pread(fd_, buffer, kPAGE_SIZE, offset)};

    if (bytes_read < 0)
        throw std::runtime_error("DiskManager: incomplete read for page: " + std::to_string(page_id));

    // fill rest of the bytes with 0 for most common case where whole page can be empty
    if (static_cast<size_t>(bytes_read) < kPAGE_SIZE)
        std::memset(buffer + bytes_read, 0, kPAGE_SIZE - bytes_read);
}

off_t DiskManager::get_size() const {
    if (fd_ == -1)
        return -1;
    struct stat sb{};
    if (fstat(fd_, &sb) == -1) {
        return -1;
    }
    return sb.st_size;
}

} // namespace duck