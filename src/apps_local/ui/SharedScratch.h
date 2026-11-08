#pragma once

// A working buffer that exists only while its app does, instead of holding
// internal RAM from boot.
//
// Internal RAM is preferred because these buffers are hot or are SD transfer
// targets, and PSRAM is slower for both; PSRAM is the fallback, so a fragmented
// internal heap does not stop the app opening. Counted because a restarting app
// is created before its predecessor is destroyed.

#include <cstddef>
#include <cstdlib>

#if defined(ARDUINO_ARCH_ESP32)
#include <esp_heap_caps.h>
#endif

namespace toybox {

class SharedScratch {
 public:
  bool acquire(const size_t bytes) {
    if (refs_ == 0) {
#if defined(ARDUINO_ARCH_ESP32)
      memory_ =
          heap_caps_malloc_prefer(bytes, 2, MALLOC_CAP_INTERNAL | MALLOC_CAP_8BIT, MALLOC_CAP_SPIRAM | MALLOC_CAP_8BIT);
#else
      memory_ = std::malloc(bytes);
#endif
      if (memory_ == nullptr) return false;
    }
    ++refs_;
    return true;
  }

  void release() {
    if (refs_ == 0 || --refs_ != 0) return;
#if defined(ARDUINO_ARCH_ESP32)
    heap_caps_free(memory_);
#else
    std::free(memory_);
#endif
    memory_ = nullptr;
  }

  void* get() const { return memory_; }
  bool fresh() const { return refs_ == 1; }

 private:
  void* memory_ = nullptr;
  int refs_ = 0;
};

}  // namespace toybox
