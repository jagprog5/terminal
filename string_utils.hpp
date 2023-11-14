#pragma once

#include <cassert>
#include <cstring>
#include <cwchar>
#include <optional>
#include <string_view>
#include <variant>
#include <vector>

#include "color.hpp"

template <typename T>
void append_to_buffer(std::vector<T>& buf, const T* begin, const T* end) {
  buf.resize(buf.size() + (end - begin));
  std::copy(begin, end, &*buf.end() - (end - begin));
}

// a utf8 character has a maximum size of 4 bytes.
static constexpr size_t MAX_BYTES_PER_CHARACTER = 4;

// this can contain invalid UTF-8. The only guarentee is that the character has
// a completed length (as indicated by it's first byte). Also it will never be empty.
struct UTF8Block {
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

  static UTF8Block stray_continuation() {
    UTF8Block ret; // https://www.fileformat.info/info/unicode/char/fffd/index.htm
    ret.data[0] = 0xEF;
    ret.data[1] = 0xBF;
    ret.data[2] = 0xBD;
    return ret;
  }

  // Used to represent an invalid UTF-8 character.
  static UTF8Block invalid_utf8() {
    UTF8Block ret; // https://www.fileformat.info/info/unicode/char/fffc/index.htm
    ret.data[0] = 0xEF;
    ret.data[1] = 0xBF;
    ret.data[2] = 0xBC;
    return ret;
  }

  // Used to represent anything that doesn't have a glyph in this font
  static UTF8Block no_glyph() {
    UTF8Block ret; // https://www.fileformat.info/info/unicode/char/25a1/index.htm
    ret.data[0] = 0xE2;
    ret.data[1] = 0x96;
    ret.data[2] = 0xA1;
    return ret;
  }

  UTF8Block() { memset(data, 0, sizeof(data)); }

  UTF8Block(const UTF8Block& other) { *this = other; }

  UTF8Block(UTF8Block&& other) { *this = std::move(other); }

  UTF8Block& operator=(UTF8Block&& other) {
    *this = other; // non move
    return *this;
  }

  UTF8Block& operator=(const UTF8Block& other) {
    // last null char doesn't need to be copied around. constant for all instances
    for (size_t i = 0; i < MAX_BYTES_PER_CHARACTER; ++i) {
      data[i] = other.data[i];
    }
    return *this;
  }

  bool operator==(const UTF8Block& other) const {
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
    // a UTF-8 version. but also the validity check is nice
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

namespace std {
template <>
struct hash<UTF8Block> {
  size_t operator()(const UTF8Block& ch) const {
    auto view = std::string_view(ch.data, MAX_BYTES_PER_CHARACTER);
    return std::hash<std::string_view>{}(view);
  }
};
} // namespace std

struct ANSICursorUp {
  uint16_t n;
};

struct ANSICursorDown {
  uint16_t n;
};

struct ANSICursorForward {
  uint16_t n;
};

struct ANSICursorBack {
  uint16_t n;
};

struct ANSICursorNextLine {
  uint16_t n;
};

struct ANSICursorPreviousLine {
  uint16_t n;
};

struct ANSICursorHorizontalAbsolute {
  uint16_t n;
};

struct ANSICursorPosition {
  uint16_t row, col;
};

struct ANSIEraseDisplay {
  unsigned char type;
};

struct ANSIEraseLine {
  unsigned char type;
};

struct ANSIScrollUp {
  uint16_t n;
};

struct ANSIScrollDown {
  uint16_t n;
};

struct ANSISaveCursor {};

struct ANSILoadCursor {};

struct ANSIGraphicsReset {};

struct ANSIGraphicsBold {};

struct ANSIGraphicsItalic {};

struct ANSIGraphicsUnderline {};

struct ANSIGraphicsForeground {
  Color c;
};

struct ANSIGraphicsBackground {
  Color c;
};

// a Block is an indivisible unit to be used in the display. it can be either
// UTF-8 (possible invalid, validity is checked before displaying), or some ansi
// escape sequence which applies various functionality
using Block = std::variant<UTF8Block,                    //
                           ANSICursorUp,                 //
                           ANSICursorDown,               //
                           ANSICursorForward,            //
                           ANSICursorBack,               //
                           ANSICursorNextLine,           //
                           ANSICursorPreviousLine,       //
                           ANSICursorHorizontalAbsolute, //
                           ANSICursorPosition,           //
                           ANSIEraseDisplay,             //
                           ANSIEraseLine,                //
                           ANSIScrollUp,                 //
                           ANSIScrollDown,               //
                           ANSISaveCursor,               //
                           ANSILoadCursor,               //
                           ANSIGraphicsReset,            //
                           ANSIGraphicsBold,             //
                           ANSIGraphicsItalic,           //
                           ANSIGraphicsUnderline,        //
                           ANSIGraphicsForeground,       //
                           ANSIGraphicsBackground>;

// consumes input over many calls, produces Blocks from the stream
class BlockStream {
  // this section of members is used when producing an ANSI escape block
  enum ANSIParseState {
    NONE,
    ESCAPE_RECEIVED,
    CSI_RECEIVED,
  } ansi_parse_state = NONE;
  static constexpr size_t MAX_ARGS = 64; // https://vt100.net/emu/dec_ansi_parser after n (16 in the citation), args are ignored
  size_t ansi_index;                     // index with args. fixed length vector
  uint16_t ansi_args[MAX_ARGS];

  // this section of members is used when producing a UTF8Block block
  char incomplete[MAX_BYTES_PER_CHARACTER]; // incomplete multibyte
  unsigned char bytes_to_complete = 0;      // number of bytes needed to complete it
  unsigned char offset = 0;                 // next available index in incomplete

  // this section of fields is used for ansi blocks

  BlockStream(const BlockStream&) = delete;
  BlockStream& operator=(const BlockStream&) = delete;
  BlockStream(BlockStream&&) = delete;
  BlockStream& operator=(BlockStream&&) = delete;

 public:
  BlockStream() {}

  std::vector<Block> consume(const char* data, size_t length) {
    std::vector<Block> ret;

    // go to appropriate position of state machine for ansi code parse
    switch (ansi_parse_state) {
      case ESCAPE_RECEIVED:
        goto escape_received_state;
        break;
      case CSI_RECEIVED:
        goto csi_received_state;
        break;
      default:
        // ansi code not parsed right now
        break;
    }

    if (bytes_to_complete != 0) { // handle the incomplete multibyte from previous call
      if (bytes_to_complete <= length) {
        // there is enough bytes to complete the incomplete multibyte
        size_t num_to_take = bytes_to_complete;
        memcpy(incomplete + offset, data, num_to_take);
        UTF8Block blk;
        memcpy(blk.data, incomplete, MAX_BYTES_PER_CHARACTER);
        ret.push_back(blk);
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
        return ret; // empty
      }
    }

next_block:
    if (length == 0) {
      return ret;
    }

    if (*data != '\e') {
      // this next block is not an ansi escape sequence. handle UTF-8 block
      int bytes_needed = UTF8Block::u8_length(*data);
      if (bytes_needed == -1) {
        ret.push_back(UTF8Block::stray_continuation());
        // progress by a single byte on error length block
        data += 1;
        length -= 1;
        goto next_block;
      }

      if (bytes_needed <= length) {
        // there is enough to complete the current character
        UTF8Block ch;
        memcpy(ch.data, data, bytes_needed);
        ret.push_back(ch);
        data += bytes_needed;
        length -= bytes_needed;
        goto next_block;
      } else {
        // not enough. place it into incomplete
        for (size_t i = 0; i < length; ++i) {
          this->incomplete[offset++] = data[i];
        }
        bytes_to_complete = bytes_needed - length;
        offset += length;
        return ret;
      }
      // <- never reached
    } // finishes UTf8-block handling

    ansi_parse_state = ESCAPE_RECEIVED;
    data += 1;
    length -= 1;
escape_received_state:
    if (length == 0) {
      return ret;
    }
    {
      bool success = *data == '[';
      data += 1;
      length -= 1;
      if (!success) {
        ansi_parse_state = NONE;
        goto next_block; // consumes the \e and one following
      }
    }
    ansi_parse_state = CSI_RECEIVED;
    ansi_index = 0;
    ansi_args[0] = 0; // some commands use 0s as default args.
    ansi_args[1] = 0; // only clear first and second
csi_received_state:
    if (length == 0) {
      return ret;
    }

    // handling the args, character by character
    if (*data >= '0' && *data <= '9') {
      if (ansi_index != MAX_ARGS) { // ignore if max args exceeded (one past end)
        ansi_args[ansi_index] *= 10;
        ansi_args[ansi_index] += *data - '0';
      }
    } else if (*data == ';') {
      if (ansi_index != MAX_ARGS) { // don't increment when at one past end
        ansi_index += 1;
        ansi_args[ansi_index] = 0;
      }
    } else if (*data == 'h') {
      if (ansi_index != MAX_ARGS) { // ignore if max args exceeded (one past end)
        ansi_args[ansi_index - 1] *= 10;
        ansi_args[ansi_index - 1] += 1;
      }
    } else if (*data == 'A') {
      ret.push_back(ANSICursorUp{ansi_args[0]});
    } else if (*data == 'B') {
      ret.push_back(ANSICursorDown{ansi_args[0]});
    } else if (*data == 'C') {
      ret.push_back(ANSICursorForward{ansi_args[0]});
    } else if (*data == 'D') {
      ret.push_back(ANSICursorBack{ansi_args[0]});
    } else if (*data == 'E') {
      ret.push_back(ANSICursorNextLine{ansi_args[0]});
    } else if (*data == 'F') {
      ret.push_back(ANSICursorPreviousLine{ansi_args[0]});
    } else if (*data == 'G' || *data == 'f') {
      ret.push_back(ANSICursorHorizontalAbsolute{ansi_args[0]});
    } else if (*data == 'H') {
      ret.push_back(ANSICursorPosition{ansi_args[0], ansi_args[1]});
    } else if (*data == 'J') {
      ret.push_back(ANSIEraseDisplay{(unsigned char)ansi_args[0]});
    } else if (*data == 'K') {
      ret.push_back(ANSIEraseLine{(unsigned char)ansi_args[0]});
    } else if (*data == 'S') {
      ret.push_back(ANSIScrollUp{ansi_args[0]});
    } else if (*data == 'T') {
      ret.push_back(ANSIScrollDown{ansi_args[0]});
    } else if (*data == 's') {
      ret.push_back(ANSISaveCursor());
    } else if (*data == 'u') {
      ret.push_back(ANSILoadCursor());
    } else if (*data == 'm') {
      // select graphics rendition
      for (size_t i = 0; i < ansi_index; ++i) {
        if (ansi_args[i] == 0) {
          ret.push_back(ANSIGraphicsReset());
        } else if (ansi_args[i] == 1) {
          ret.push_back(ANSIGraphicsBold());
        } else if (ansi_args[i] == 3) {
          ret.push_back(ANSIGraphicsItalic());
        } else if (ansi_args[i] >= 30 && ansi_args[i] <= 37) {
          ret.push_back(ANSIGraphicsForeground{Color::from8(ansi_args[i] - 30)});
        } else if (ansi_args[i] == 38) {
          if (i + 1 < ansi_index) {
            // next index is valid (check for 2 or 5)
            if (ansi_args[i + 1] == 5) {
              // foreground via 256 palette
              if (i + 2 < ansi_index) {
                // next next index is valid
                ret.push_back(ANSIGraphicsForeground{Color::from256(ansi_args[i + 2])});
              } else {
                break;
              }
            } else if (ansi_args[i + 1] == 2) {
              // foreground via rgb
              if (i + 4 < ansi_index) {                    // next, next 3 indices are valid
                ret.push_back(ANSIGraphicsForeground{Color{//
                                                           (unsigned char)ansi_args[i + 2],
                                                           (unsigned char)ansi_args[i + 3],
                                                           (unsigned char)ansi_args[i + 4]}});
              } else {
                break;
              }
            }
          } else {
            // would normally break anyways since this is last index
          }
        } else if (ansi_args[i] == 48) {
          // copy paste of above but background instead of foreground
          if (i + 1 < ansi_index) {
            // next index is valid (check for 2 or 5)
            if (ansi_args[i + 1] == 5) {
              // background via 256 palette
              if (i + 2 < ansi_index) {
                // next next index is valid
                ret.push_back(ANSIGraphicsBackground{Color::from256(ansi_args[i + 2])});
              } else {
                break;
              }
            } else if (ansi_args[i + 1] == 2) {
              // background via rgb
              if (i + 4 < ansi_index) {                    // next, next 3 indices are valid
                ret.push_back(ANSIGraphicsBackground{Color{//
                                                           (unsigned char)ansi_args[i + 2],
                                                           (unsigned char)ansi_args[i + 3],
                                                           (unsigned char)ansi_args[i + 4]}});
              } else {
                break;
              }
            }
          } else {
            // would normally break anyways since this is last index
          }
        } else if (ansi_args[i] >= 90 && ansi_args[i] <= 97) {
          ret.push_back(ANSIGraphicsForeground{Color::from8bright(ansi_args[i] - 90)});
        } else if (ansi_args[i] >= 100 && ansi_args[i] <= 107) {
          ret.push_back(ANSIGraphicsBackground{Color::from8bright(ansi_args[i] - 100)});
        } else {
          // unknown. ignore, continue
        }
      }
    } else if (*data == '\e') {
      ansi_parse_state = ESCAPE_RECEIVED;
      data += 1;
      length -= 1;
      goto escape_received_state;
    } else {
      ansi_parse_state = NONE;
      data += 1;
      length -= 1;
      goto next_block;
    }
    data += 1;
    length -= 1;
    goto csi_received_state; // next character
  }
};

struct CellAttributes {
  Color fg, bg;
  bool italic = false;
  bool bold = false;
  bool underline = false;
};

struct Cell {
  UTF8Block blk;
  CellAttributes attributes;
};
