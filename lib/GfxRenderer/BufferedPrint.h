#pragma once

#include <Memory.h>
#include <Print.h>

#include <algorithm>
#include <cstddef>
#include <cstdint>
#include <cstring>
#include <memory>

// Gathers small writes into blocks before passing them on. An SD write costs
// about the same up to a few sectors (some 0.25 ms per call on the Picco),
// while BMP writers emit a palette byte by byte and pixels a row at a time.
// The block is heap memory sized by the caller and freed with the writer; if
// it cannot be allocated, writes go straight through. Call flushBuffer() before
// closing the destination: its result covers every write since the last one.
class BufferedPrint final : public Print {
 public:
  BufferedPrint(Print& out, const size_t capacity)
      : out_(out), buffer_(makeUniqueNoThrow<uint8_t[]>(capacity)), capacity_(buffer_ ? capacity : 0) {}
  BufferedPrint(const BufferedPrint&) = delete;
  BufferedPrint& operator=(const BufferedPrint&) = delete;

  using Print::write;
  size_t write(const uint8_t value) override { return write(&value, 1); }
  size_t write(const uint8_t* data, const size_t size) override {
    if (failed_) return 0;
    if (capacity_ == 0) {
      const size_t written = out_.write(data, size);
      failed_ = written != size;
      return written;
    }
    size_t done = 0;
    while (done < size) {
      if (used_ == capacity_ && !flushBuffer()) return done;
      const size_t chunk = std::min(size - done, capacity_ - used_);
      memcpy(buffer_.get() + used_, data + done, chunk);
      used_ += chunk;
      done += chunk;
    }
    return done;
  }

  // Passes the buffered bytes on. False once any write has come up short.
  bool flushBuffer() {
    if (!failed_ && used_ > 0) failed_ = out_.write(buffer_.get(), used_) != used_;
    used_ = 0;
    return !failed_;
  }

 private:
  Print& out_;
  std::unique_ptr<uint8_t[]> buffer_;
  size_t capacity_;
  size_t used_ = 0;
  bool failed_ = false;
};
