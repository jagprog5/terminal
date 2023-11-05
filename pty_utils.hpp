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

#include <cwchar>
#include <deque>
#include <optional>
#include <utility>
#include <vector>

#include <atomic>
#include <mutex>
#include <thread>

#include "sdl_utils.hpp"


static const char* SHELL = "/bin/dash";

#define TERM_NAME "not_named_yet"
#define GENERAL_TIMEOUT_MS 30

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
  // doesn't return if this is the slave process.
  // returns if this is the master process
  bool spawn() {
    const char* const env[] = {"TERM=" TERM_NAME, NULL};

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

    RendererPtr r = create_renderer(w);
    if (!r) {
      return false;
    }

    // text from the slave process is read in reader_thread,
    // and passed to the main thread via SDL events of this type.
    const Uint32 read_event_type = SDL_RegisterEvents(1);
    if (read_event_type == -1) {
      return false;
    }

    std::optional<CharacterManager> maybe_cm = CharacterManager::create(sdl_context);
    if (!maybe_cm) {
      return false;
    }
    CharacterManager& character_manager = *maybe_cm;

    // flag to shut down
    std::atomic<bool> quit = false;

    // mutual exclusion on this->master. this is a paranoia measure, insulating
    // from the read/write implementation on the os
    std::mutex master_lock;

    std::thread reader_thread([&quit, &master_lock, &master = this->master, read_event_type]() {
      while (!quit) {
        static constexpr size_t BUF_MAX_SIZE = 256;
        void* buffer = malloc(sizeof(char) * BUF_MAX_SIZE);
        if (buffer == NULL) {
          perror("malloc buf");
          quit = true; // signal to main thread
          break;
        }

        ssize_t size;
        {
          std::lock_guard lk(master_lock);
          // note that master was set to non-blocking mode
          size = read(master, buffer, BUF_MAX_SIZE);
        }

        if (size == -1) {
          // Check if the read operation would block
          if (errno == EAGAIN || errno == EWOULDBLOCK) {
            // no input yet. delay to not spinlock
            std::this_thread::sleep_for(std::chrono::milliseconds(GENERAL_TIMEOUT_MS));
            continue;
          } else {
            perror("read pts");
            quit = true; // signal to main thread
            break;
          }
        }
        // buffer is filled with size bytes at this point (256 allocated but maybe not all used)

        SDL_Event event;
        SDL_zero(event); // memset
        event.user.type = read_event_type;

        // data1 points to malloc'ed char* of text read
        event.user.data1 = buffer;

        // cast is safe since size will never by negative, and uintptr_t's max is greater than int32 max
        uintptr_t num_bytes = size;
        // data2 is not a pointer, it encodes the length of bytes used in data1
        event.user.data2 = reinterpret_cast<void*>(num_bytes);

        SDL_PushEvent(&event); // explicitely thread safe. copies event onto queue
      }
    });

    while (!quit) {
      // handle inputs
      SDL_Event event;

      while (SDL_WaitEventTimeout(&event, GENERAL_TIMEOUT_MS)) {
        if (event.type == read_event_type) {
          struct char_ptr_free {
            void operator()(char* p) { free(p); } // null check not required
          };
          auto data = std::unique_ptr<char, char_ptr_free>(static_cast<char*>(event.user.data1));
          uintptr_t size = reinterpret_cast<uintptr_t>(event.user.data2);

          // display the data received by the slave

          continue;
        }
        switch (event.type) {
          case SDL_QUIT:
            quit = true; // signal to reader thread
            break;
          case SDL_TEXTINPUT:
            // send the characters to the slave device
            puts("yup");
            // null terminating utf8
            // event.text.text
            break;
          case SDL_KEYDOWN:
            // event.key
            break;
          case SDL_KEYUP:
            break;
          default:
            break;
        }
      }
      // draw
      SDL_SetRenderDrawColor(r.get(), 0, 0, 0, 255);
      SDL_RenderClear(r.get());

      SDL_SetRenderDrawColor(r.get(), 255, 0, 0, 255);
      SDL_Rect rect = {10, 10, 10, 10};
      SDL_RenderDrawRect(r.get(), &rect);
      SDL_RenderPresent(r.get());
    }

    reader_thread.join();
    return true;
  }
};
