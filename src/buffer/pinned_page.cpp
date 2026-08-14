#include "duck/buffer/pinned_page.hpp"
#include "duck/buffer/pool_manager.hpp"

namespace duck {

PinnedPage make_pinned(Page* page, BufferPoolManager* bpm) {
    return PinnedPage{page, bpm};
}

} // namespace duck