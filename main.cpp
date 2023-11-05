#include "pty_utils.hpp"
#include "sdl_utils.hpp"

int main(int argc, const char* const* argv) {
  std::optional<PTY> maybe_pty = PTY::create();
  if (!maybe_pty) {
    return 1;
  }
  PTY& pty = *maybe_pty;

  if (!pty.spawn()) {
    return 1; 
  }

  std::optional<SDLContext> maybe_context = SDLContext::create();
  if (!maybe_context) {
    return 1;
  }

  SDLContext& sdl_context = *maybe_context;
  if (!pty.run(sdl_context)) {
    return 1;
  }
  return 0;
}
