#pragma once

// #include <cwchar>
#include <cuchar>
#include <vector>

template <typename T>
void append_to_buffer(std::vector<T>& buf, const T* begin, const T* end) {
  buf.resize(buf.size() + (end - begin));
  std::copy(begin, end, &*buf.end() - (end - begin));
}

class UTF8DecodeStream {
  std::mbstate_t state = std::mbstate_t();
  std::vector<char> queue;

 public:
  std::vector<char16_t> get(const char* data_arg, size_t length_arg) {
    std::vector<char16_t> ret;

    bool no_queue = this->queue.empty();

    const char* data;
    size_t length;

    if (no_queue) {
      data = data_arg;
      length = length_arg;
    } else {
      append_to_buffer(this->queue, data_arg, data_arg + length_arg);
      data = this->queue.data();
      length = this->queue.size();
    }

    while(1) {
      if (length == 0) {
        if (!no_queue) {
          this->queue.clear();
        }
        return ret;
      }

      char16_t wc;
      size_t bytes_used = std::mbrtoc16(&wc, data, length, &this->state);

      if (bytes_used == (size_t)-2) { // ended with incomplete multibyte
        if (no_queue) {
          append_to_buffer(this->queue, data, data + length);
        } else {
          this->queue.erase(this->queue.begin(), this->queue.end() - length);
        }
        return ret;
      }

      if (bytes_used == (size_t)-1) { // decode error
        this->state = std::mbstate_t();
        bytes_used = 1; // just keep going and make progress
        wc = L'�';
      }

      if (bytes_used == 0) { // null char decoded
        bytes_used = 1; // this shouldn't have a special return value
      }

      if (bytes_used == (size_t)-3) {
        bytes_used = 0; // TODO THINK
      }

      data += bytes_used;
      length -= bytes_used;
      ret.push_back(wc);
    }
  }
};
