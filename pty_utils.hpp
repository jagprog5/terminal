#pragma once

// guided by https://www.uninformativ.de/git/eduterm/file/eduterm.c.html

#include <errno.h>
#include <fcntl.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/ioctl.h>
#include <sys/wait.h>
#include <unistd.h>

#include <optional>
#include <utility>
#include <vector>

#include <thread>

#include "sdl_utils.hpp"
#include "string_utils.hpp"

static const char* SHELL = "/bin/sh";

#define TERM_NAME "not_named_yet"

// raii wrapper of file descriptor
class FileDescriptor {
  int fd = -1;

 public:
  FileDescriptor(const char* path, int flags) noexcept : fd(::open(path, flags)) {}

  FileDescriptor(const FileDescriptor&) = delete;
  FileDescriptor& operator=(const FileDescriptor&) = delete;
  FileDescriptor& operator=(FileDescriptor&& other) = delete;

  FileDescriptor(FileDescriptor&& other) noexcept : fd(other.fd) { //
    other.fd = -1;
  }

  explicit operator bool() const noexcept { return this->fd != -1; }
  operator int() const noexcept { return this->fd; }

  // only explicitly close when necessary.
  // otherwise, allow dtor to close.
  bool close() noexcept {
    if (*this) {
      bool close_success = ::close(*this) != -1;
      this->fd = -1;
      return close_success;
    }
    return true;
  }

  ~FileDescriptor() {
    if (!close()) {
      // log failure. no other action required or available given dtor context
      perror("err fd close");
    }
  }
};

class PTY {
  FileDescriptor master;
  FileDescriptor slave;

  PTY(FileDescriptor master, FileDescriptor slave) : master(std::move(master)), slave(std::move(slave)) {}

 public:
  // empty for failure: error reason printed
  static std::optional<PTY> create() {
    // master is non-blocking
    // this is required for run() logic
    FileDescriptor master("/dev/ptmx", O_RDWR | O_NOCTTY | O_NONBLOCK);

    if (!master) {
      perror("err open /dev/ptmx");
      return {};
    }

    if (grantpt(master) < 0) {
      perror("err grant /dev/ptmx");
      return {};
    }

    if (unlockpt(master) < 0) {
      perror("err unlock /dev/ptmx");
      return {};
    }

    const char* slave_path = ptsname(master);
    if (slave_path == NULL) {
      perror("ptsname");
      return {};
    }

    FileDescriptor slave(slave_path, O_RDWR | O_NOCTTY);
    if (!slave) {
      fprintf(stderr, "err open %s: %s\n", slave_path, strerror(errno));
      return {};
    }

    return PTY(std::move(master), std::move(slave));
  }

  // false on failure (err printed).
  // forks. doesn't return if this is the slave process.
  // returns if this is the master process
  bool spawn() {
    const char* const env[] = {"TERM=xterm-256color", NULL};

    pid_t pid = fork();

    if (pid == -1) {
      perror("err fork");
      return false;
    }

    if (pid == 0) {
      // child. this is the slave process. close access to master
      this->master.close(); // deliberately ignore return

      // make this the session leader
      if (setsid() == -1) {
        perror("err setsid");
        return false;
      }

      // and the controlling terminal
      int ret = ioctl(this->slave, TIOCSCTTY, NULL);
      if (ret == -1) {
        // if master fd's handles have been closed (from master process
        // exiting already, which can happen if it exits fast enough), ioctl
        // results in EIO
        perror("err ioctl(TIOCSCTTY)");
        return false;
      }

      // setup my streams to be the same as the pts
      for (int i = 0; i < 3; ++i) {
        if (dup2(this->slave, i) == -1) {
          perror("dup2");
          return false;
        }
      }

      this->slave.close();

      // replace me with the shell
      execle(SHELL, SHELL, NULL, env);
      // never reached normally
      perror("err exec");
      return false;
    } else {
      // parent. pid is the child's pid
      // this is the master process. close access to slave
      this->slave.close(); // deliberately ignore return
      return true;
    }
  }

  bool run(const SDLContext& sdl_context) {
    WindowPtr w = sdl_context.create_window(TERM_NAME);
    if (!w) {
      return false;
    }

    RendererPtr renderer = create_renderer(w);
    if (!renderer) {
      return false;
    }

    std::optional<CharacterManager> maybe_cm = CharacterManager::create(sdl_context);
    if (!maybe_cm) {
      return false;
    }
    CharacterManager& character_manager = *maybe_cm; // texture cache for character rendering

    BlockStream block_stream;

    // the lines to display in the terminal
    std::vector<std::vector<Cell>> lines;
    lines.emplace_back(); // lines will never by empty

    CellAttributes cursor_attributes;
    // position of where text received from the shell will be drawn next
    int cursor_x = 0; // pixels (right from left of screen)
    int cursor_y = 0; // pixels (down from top of screen)

    // position in lines for when the entire screen is redrawn
    // terminology is confusing. a "line" (std::vector<Cell>) is broken by newline chars received by the shell.
    // however, the line itself is broken up into lines visually when text wrapping occurs.
    // lines[start_line][start_cell] is where drawing starts at the top left of the screen
    int start_line = 0;
    int start_cell = 0;

    // this is the position in lines where text is currently inserted. unlike cursor position,
    // it is not effected by screen wrapping
    int insert_line_pos = 0;
    int insert_cell_pos = 0;

    std::vector<char> master_write_q; // used by write_txt_to_shell

    // a helper lambda. if all the data hasn't been written by write syscall, rather
    // than looping until it's finished writing, it gets put on a queue which is
    // written first on subsequent calls. this ensures calling has bounded time
    auto write_txt_to_shell = [&](const char* text, size_t length) -> bool {
      // returns negative value on error (error printed)
      auto do_write = [&](const char* text, size_t length) -> ssize_t {
        ssize_t bytes_written = write(this->master, text, length);
        if (bytes_written < 0) {
          if (errno == EAGAIN || errno == EWOULDBLOCK) {
            bytes_written = 0; // no bytes written
          } else {
            perror("write pts"); // negative return handled by callee
          }
        }
        return bytes_written;
      };

      if (master_write_q.empty()) { // ================ no text is backed up ==========
        ssize_t bytes_written = do_write(text, length);
        if (bytes_written < 0) {
          return false;
        }
        if (bytes_written != length) {
          text += bytes_written;
          length -= bytes_written;
          append_to_buffer(master_write_q, text, text + length);
        }
      } else { // ==================================== text is backed up ==============
        append_to_buffer(master_write_q, text, text + length);
        ssize_t bytes_written = do_write(master_write_q.data(), master_write_q.size());
        if (bytes_written < 0) {
          return false;
        }
        // remove the written bytes from the q
        master_write_q.erase(master_write_q.begin(), master_write_q.begin() + bytes_written);
      }
      return true;
    };

    auto render_cell = [&](int x, int y, const Cell& cell) {
      SDL_Texture* texture = cell.texture;
      SDL_Rect dst{x, y, CELL_WIDTH, CELL_HEIGHT};
      // background
      SDL_SetRenderDrawColor(renderer.get(), cell.attributes.bg.r, cell.attributes.bg.g, cell.attributes.bg.b, 255);
      SDL_RenderFillRect(renderer.get(), &dst);
      // foreground
      SDL_SetTextureColorMod(texture, cell.attributes.fg.r, cell.attributes.fg.g, cell.attributes.fg.b);
      SDL_RenderCopy(renderer.get(), texture, NULL, &dst);
    };

    while (1) { // main loop
      bool full_redraw_required = false;
      SDL_Event event;                        // ============================ SDL handle event ===============
      unsigned int poll_event_per_iter = 100; // ensure main loop is bounded
      while (--poll_event_per_iter && SDL_PollEvent(&event)) {
        if (event.type == SDL_QUIT) {
          goto break_topmost;
        } else if (event.type == SDL_TEXTINPUT) {
          if (!write_txt_to_shell(event.text.text, strlen(event.text.text))) {
            goto break_topmost; // error already printed
          }
        } else if (event.type == SDL_KEYDOWN) {
          // text input is for text only. it doesn't work for things like backspace or enter
          char simple_typed = '\0';
          switch (event.key.keysym.sym) {
            case SDLK_BACKSPACE:
              simple_typed = '\b';
              break;
            case SDLK_RETURN:
              simple_typed = '\n';
            default:
              break;
          }

          if (simple_typed != '\0') {
            if (!write_txt_to_shell(&simple_typed, 1)) {
              goto break_topmost; // error already printed
            }
          }
        } else if (event.type == SDL_MOUSEWHEEL) {
          // assuming each character spans 1 cell. not true in reality, but this is ignored
          if (event.wheel.y < 0) { // scroll down
            for (int i = 0; i < -event.wheel.y; ++i) {
              start_cell += CELLS_PER_WIDTH;
              cursor_y -= CELL_HEIGHT;
              if (start_line < 0 || start_line >= lines.size() || start_cell >= lines[start_line].size()) {
                start_line += 1;
                start_cell = 0;
              }
            }
          } else { // scroll up
            // negative scroll is scroll up
            for (int i = 0; i < event.wheel.y; ++i) {
              start_cell -= CELLS_PER_WIDTH;
              cursor_y += CELL_HEIGHT;
              if (start_cell < 0) {
                start_line -= 1;
                if (start_line < 0 || start_line >= lines.size()) {
                  start_cell = 0;
                } else {
                  if (CELLS_PER_WIDTH == 0) {
                    start_cell = 0;
                  } else {
                    start_cell = (lines[start_line].size() / CELLS_PER_WIDTH) * CELLS_PER_WIDTH;
                  }
                }
              }
            }
          }
          full_redraw_required = true;
        } else {
          // TODO other events like window resize handling
        }
      }

      if (full_redraw_required) {
        SDL_RenderClear(renderer.get());
        int redraw_cursor_x = 0; // pixels
        int redraw_cursor_y = 0;

        int line_index = start_line;
        while (1) {
          if (line_index >= 0 && line_index < lines.size()) {
            const std::vector<Cell>& line = lines[line_index];
            assert(start_cell >= 0);
            for (int cell_index = line_index == start_line ? start_cell : 0; cell_index < line.size(); ++cell_index) {
              render_cell(redraw_cursor_x, redraw_cursor_y, line[cell_index]);
              redraw_cursor_x += CELL_WIDTH;
              if (redraw_cursor_x >= SCREEN_WIDTH) {
                redraw_cursor_x = 0;
                redraw_cursor_y += CELL_HEIGHT;
                if (redraw_cursor_y >= SCREEN_HEIGHT) {
                  goto break_full_redraw;
                }
              }
            }
          }
          ++line_index;
          redraw_cursor_y += CELL_HEIGHT;
          redraw_cursor_x = 0;
          if (redraw_cursor_y >= SCREEN_HEIGHT) {
            break;
          }
        }
break_full_redraw:
        SDL_RenderPresent(renderer.get());
      }

      static constexpr size_t BUF_MAX_SIZE = 256; // ============= pts read ===========
      char buffer[BUF_MAX_SIZE];
      ssize_t bytes_read = read(master, buffer, BUF_MAX_SIZE);
      if (bytes_read < 0) {
        if (errno == EAGAIN || errno == EWOULDBLOCK) {
          bytes_read = 0;
        } else if (errno == EIO) {
          break; // shell shut down
        } else {
          perror("read pts");
          break;
        }
      }

      std::vector<Block> blocks = block_stream.consume(buffer, bytes_read);

      // helper lambda
      auto insert_cell = [&](Cell cell) {
        render_cell(cursor_x, cursor_y, cell);
        cursor_x += CELL_WIDTH; // move to next position
        if (cursor_x >= SCREEN_WIDTH) {
          cursor_x = 0;
          cursor_y += CELL_HEIGHT;
        }

        assert(insert_line_pos >= 0 && insert_line_pos < lines.size());
        while (insert_cell_pos >= lines[insert_line_pos].size()) {
          // insert a default space until we reach the position in this line
          lines[insert_line_pos].push_back({character_manager.get(UTF8Block::space(), renderer), CellAttributes()});
        }

        assert(insert_cell_pos >= 0 && insert_cell_pos < lines[insert_line_pos].size());
        lines[insert_line_pos][insert_cell_pos] = cell; // replace
        insert_cell_pos += 1;
      };

      if (!blocks.empty()) {
        for (const Block& blk : blocks) {
          // helper lambda
          auto move_down = [&](){
            cursor_y += CELL_HEIGHT;
            insert_cell_pos += CELLS_PER_WIDTH;
            assert(insert_line_pos >= 0 && insert_line_pos <= lines.size());
            if (insert_cell_pos >= lines[insert_line_pos].size()) {
              insert_line_pos += 1;
              if (lines.size() == insert_line_pos) {
                lines.emplace_back();
              }
              if (CELLS_PER_WIDTH == 0) {
                insert_cell_pos = 0;
              } else {
                insert_cell_pos = insert_cell_pos - (insert_cell_pos / CELLS_PER_WIDTH) * CELLS_PER_WIDTH;
              }
            }
          };

          if (const UTF8Block* utf8_block = std::get_if<UTF8Block>(&blk)) {
            if (utf8_block->data[0] == '\n') {
              move_down();
            } else if (utf8_block->data[0] == '\a') {
              // no beep implemented
            } else if (utf8_block->data[0] == '\b') {
              cursor_x -= CELL_WIDTH;
              if (cursor_x < 0) {
                cursor_x = CELL_WIDTH * (CELLS_PER_WIDTH - 1);
                cursor_y -= CELL_HEIGHT;
                if (cursor_y < 0) {
                  cursor_x = 0;
                  cursor_y = 0;
                }
              }
              insert_cell_pos -= 1;
              if (insert_cell_pos < 0) {
                insert_line_pos -= 1;
                if (insert_line_pos < 0) {
                  insert_cell_pos = 0;
                  insert_line_pos = 0;
                }
              }
            } else if (utf8_block->data[0] == '\r') {
              cursor_x = 0;
              insert_cell_pos = (insert_cell_pos / CELLS_PER_WIDTH) * CELLS_PER_WIDTH;
            } else if (utf8_block->data[0] == '\t') {
              insert_cell({character_manager.get(UTF8Block::space(), renderer), cursor_attributes});
              while ((cursor_x / CELL_WIDTH) % 8 != 0) {
                insert_cell({character_manager.get(UTF8Block::space(), renderer), cursor_attributes});
              }
            } else if (utf8_block->data[0] == '\0') {
              // ignore
            } else {
              insert_cell({character_manager.get(*utf8_block, renderer), cursor_attributes});
            }
          } else if (const ANSICursorDown* cursor_down = std::get_if<ANSICursorDown>(&blk)) {
            for (decltype(cursor_down->n) i = 0; i < cursor_down->n; ++i) {
              move_down();
            }
          } else if (const ANSIGraphicsForeground* graphics_foreground_block = std::get_if<ANSIGraphicsForeground>(&blk)) {
            cursor_attributes.fg = graphics_foreground_block->c;
          } else if (const ANSIGraphicsBackground* graphics_background_block = std::get_if<ANSIGraphicsBackground>(&blk)) {
            cursor_attributes.bg = graphics_background_block->c;
          } else if (const ANSIEraseDisplay* erase_display_block = std::get_if<ANSIEraseDisplay>(&blk)) {
            if (erase_display_block->type == 2) { // entire screen
              SDL_RenderClear(renderer.get());
              cursor_attributes = CellAttributes();
              cursor_x = 0;
              cursor_y = 0;
              start_cell = 0;
              start_line = 0;
              insert_cell_pos = 0;
              insert_line_pos = 0;
              lines.clear();
              lines.emplace_back();
            } else {
              // TODO part of screen
            }
          } else if (const ANSIGraphicsReset* reset_graphics_block = std::get_if<ANSIGraphicsReset>(&blk)) {
            cursor_attributes = CellAttributes();
          } else {
            // TODO
          }
        }
        SDL_RenderPresent(renderer.get());
      }

      // everything in the while loop is non blocking. don't consume entire core
      std::this_thread::sleep_for(std::chrono::milliseconds(20));
    }
break_topmost:
    return true;
  }
};
