#pragma once

#include <cwchar>
#include <string_view>
#include <vector>
#include <optional>
#include <cassert>

template <typename T>
void append_to_buffer(std::vector<T>& buf, const T* begin, const T* end) {
  buf.resize(buf.size() + (end - begin));
  std::copy(begin, end, &*buf.end() - (end - begin));
}

// a utf8 character has a maximum size of 4 bytes.
static constexpr size_t MAX_BYTES_PER_CHARACTER = 4;

struct UTF8Character {
  // +1 so it's always null terminating, even at largest length
  char data[MAX_BYTES_PER_CHARACTER + 1];

  // c is the first byte of a utf8 multibyte sequence. returns the length of the multibyte
  // returns -1 on error. e.g. this is a continuation byte
  static int u8_length(unsigned char c) {
    if (c < 0b10000000) {
      return 1;
    } else if ((c & 0b11100000) == 0b11000000) {
      return 2;
    } else if ((c & 0b11110000) == 0b11100000) {
      return 3;
    } else if ((c & 0b11111000) == 0b11110000) {
      return 4;
    } else {
      return -1;
    }
  }

  static UTF8Character stray_continuation() {
    UTF8Character ret; // https://www.fileformat.info/info/unicode/char/fffd/index.htm
    ret.data[0] = 0xEF;
    ret.data[1] = 0xBF;
    ret.data[2] = 0xBD;
    return ret;
  }

  // Used to represent an invalid UTF-8 character.
  static UTF8Character invalid_utf8() {
    UTF8Character ret; // https://www.fileformat.info/info/unicode/char/fffc/index.htm
    ret.data[0] = 0xEF;
    ret.data[1] = 0xBF;
    ret.data[2] = 0xBC;
    return ret;
  }

  // Used to represent anything that doesn't have a glyph in this font
  static UTF8Character no_glyph() {
    UTF8Character ret; // https://www.fileformat.info/info/unicode/char/25a1/index.htm
    ret.data[0] = 0xE2;
    ret.data[1] = 0x96;
    ret.data[2] = 0xA1;
    return ret;
  }

  UTF8Character() { memset(data, 0, sizeof(data)); }

  UTF8Character(const UTF8Character& other) {
    *this = other;
  }
  
  UTF8Character(UTF8Character&& other) {
    *this = std::move(other);
  }

  UTF8Character& operator=(UTF8Character&& other) {
    *this = other; // non move
    return *this;
  }

  UTF8Character& operator=(const UTF8Character& other) {
    // last null char doesn't need to be copied around. constant for all instances
    for (size_t i = 0; i < MAX_BYTES_PER_CHARACTER; ++i) {
      data[i] = other.data[i];
    }
    return *this;
  }
  
  bool operator==(const UTF8Character& other) const {
    for (size_t i = 0; i < MAX_BYTES_PER_CHARACTER; ++i) {
      if (data[i] != other.data[i]) {
        return false;
      }
    }
    return true;
  };

  // returns empty if not valid UTF-8
  std::optional<wchar_t> to_wc() const {
    // this is only needed because TTF_GlyphIsProvided32 doesn't have
    // a UTF-8 version.
    wchar_t ret;
    std::mbstate_t ctx = mbstate_t();
    size_t result = mbrtowc(&ret, this->data, MAX_BYTES_PER_CHARACTER, &ctx);
    assert(result != (size_t)-2 && result != (size_t)0);
    if (result == (size_t)-1) {
      return {};
    }
    return ret;
  }
};

class UTF8Stream {
  char incomplete[MAX_BYTES_PER_CHARACTER]; // incomplete multibyte
  unsigned char bytes_to_complete = 0;      // number of bytes needed to complete it
  unsigned char offset = 0;                 // next available index in incomplete

  UTF8Stream(const UTF8Stream&) = delete;
  UTF8Stream& operator=(const UTF8Stream&) = delete;
  UTF8Stream(UTF8Stream&&) = delete;
  UTF8Stream& operator=(UTF8Stream&&) = delete;

 public:
  UTF8Stream() {}

  // this doesn't have any notion of validity. only the length of UTF-8
  // characters is understood and handled
  std::vector<UTF8Character> consume(const char* data, size_t length) {
    // not needed as existing logic handles it appropriately,
    // however this case happens often due to non blocking read
    // if (length == 0) {
    //   return {};
    // }

    std::vector<UTF8Character> ret;

    if (bytes_to_complete != 0) { // handle the incomplete multibyte from previous call
      if (bytes_to_complete <= length) {
        // there is enough bytes to complete the incomplete multibyte
        size_t num_to_take = bytes_to_complete;
        memcpy(incomplete + offset, data, num_to_take);
        ret.emplace_back();
        memcpy(ret.rbegin()->data, incomplete, MAX_BYTES_PER_CHARACTER);
        offset = 0;
        bytes_to_complete = 0;
        data += num_to_take;
        length -= num_to_take;
      } else {
        // there is not enough bytes to complete the so far incomplete multibyte
        size_t num_to_take = length;
        memcpy(incomplete + offset, data, num_to_take);
        offset += num_to_take;
        bytes_to_complete -= num_to_take;
        return {};
      }
    }

    while (length != 0) {
      int bytes_needed = UTF8Character::u8_length(*data);
      if (bytes_needed == -1) {
        ret.push_back(UTF8Character::stray_continuation());
        // progress by a single byte
        data += 1;
        length -= 1;
        continue;
      }

      if (bytes_needed <= length) {
        // there is enough to complete the current character
        UTF8Character ch;
        memcpy(ch.data, data, bytes_needed);
        ret.push_back(ch);
        data += bytes_needed;
        length -= bytes_needed;
      } else {
        // not enough. place it into incomplete
        for (size_t i = 0; i < length; ++i) {
          this->incomplete[offset++] = data[i];
        }
        bytes_to_complete = bytes_needed - length;
        offset += length;
        break;
      }
    }

    return ret;
  }
};

namespace std {
template <>
struct hash<UTF8Character> {
  size_t operator()(const UTF8Character& ch) const {
    auto view = std::string_view(ch.data, MAX_BYTES_PER_CHARACTER);
    return std::hash<std::string_view>{}(view);
  }
};
} // namespace std
