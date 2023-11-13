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

#include "string_utils.hpp"
#include "sdl_utils.hpp"

static const char* SHELL = "/bin/dash";

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
        if (errno == EIO) {
          // if master fd's handles have been closed (from master process
          // exiting already, which can happen if it exits fast enough), ioctl
          // results in EIO
          perror("pty controller has already exited while setting up controllee. ioctl(TIOCSCTTY)");
        } else {
          perror("err ioctl(TIOCSCTTY)");
        }
        return false;
      }

      // setup my streams to be the same as the pts,
      // and replace me with the shell
      for (int i = 0; i < 3; ++i) {
        if (dup2(this->slave, i) == -1) {
          perror("dup2");
          return false;
        }
      }

      this->slave.close();

      // replace this process with shell.
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

    UTF8Stream character_stream;

    // the lines to display in the terminal
    std::vector<std::vector<UTF8Character>> lines;
    lines.emplace_back(); // lines will never by empty

    std::vector<char> master_write_q; // used by write_txt_to_shell

    // a helper lambda. if all the data hasn't been written by write syscall, rather
    // than looping until it's finished writing, it gets put on a queue which is
    // written first on subsequent calls. this ensures calling has bounded time
    auto write_txt_to_shell = [&](const char* text, size_t length) -> bool {
      // returns negative value on error
      auto do_write = [&](const char* text, size_t length) -> ssize_t {
        ssize_t bytes_written = write(this->master, text, length);
        if (bytes_written < 0) {
          if (errno == EAGAIN || errno == EWOULDBLOCK) {
            bytes_written = 0; // no bytes written
          } else {
            perror("write pts"); // allows negative returned
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

    while (1) {
      bool display_update_required = false;
      SDL_Event event; // ============================ SDL handle event ===============
      // todo add bound to this while loop, n event only before progressing to read / render
      while (SDL_PollEvent(&event)) {
        if (event.type == SDL_QUIT) {
          goto break_topmost;
        } else if (event.type == SDL_TEXTINPUT) {
          if (!write_txt_to_shell(event.text.text, strlen(event.text.text))) {
            goto break_topmost; // error already printed
          }
        } else if (event.type == SDL_KEYDOWN) {
          // text input doesn't work for things like backspace or enter
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
        } else {
          // TODO other events like window resize handling
        }
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
      std::vector<UTF8Character> chars = character_stream.consume(buffer, bytes_read);
      if (!chars.empty()) {
        display_update_required = true;

        for (UTF8Character& ch : chars) {
          if (ch.data[0] == '\n') {
            lines.emplace_back();
          } else if (ch.data[0] == L'\r') {
            // many todos
          } else if (ch.data[0] == L'\0') {
            // do not render
          } else {
            lines.rbegin()->push_back(ch);
          }
        }
      }

      if (display_update_required) {
          SDL_RenderClear(renderer.get());
          unsigned int y = 0;
          for (const std::vector<UTF8Character>& line : lines) {
            unsigned int x = 0;
            for (const UTF8Character& ch : line) {
              SDL_Texture* texture = character_manager.get(ch, renderer);
              SDL_Rect dst{(int)x, (int)y, SINGLE_CHAR_WIDTH, SINGLE_CHAR_HEIGHT};
              SDL_RenderCopy(renderer.get(), texture, NULL, &dst);
              x += SINGLE_CHAR_WIDTH;
            }
            y += SINGLE_CHAR_HEIGHT;
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
