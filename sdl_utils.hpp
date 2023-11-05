#pragma once

#include <SDL2/SDL.h>
#include <SDL2/SDL_ttf.h>
#include <stdio.h>
#include <optional>
#include <string_view>
#include <unordered_map>

#include "font_utils.hpp"
#include "mem_utils.hpp"

static constexpr unsigned int SINGLE_CHAR_WIDTH = 8;
static constexpr unsigned int SINGLE_CHAR_HEIGHT = 16;
static constexpr unsigned int SCREEN_WIDTH = SINGLE_CHAR_WIDTH * 80;
static constexpr unsigned int SCREEN_HEIGHT = SINGLE_CHAR_HEIGHT * 24;

UNIQUE_PTR_WRAPPER(WindowPtr, SDL_Window, SDL_DestroyWindow)
UNIQUE_PTR_WRAPPER(FontPtr, TTF_Font, TTF_CloseFont)
UNIQUE_PTR_WRAPPER(RendererPtr, SDL_Renderer, SDL_DestroyRenderer)
UNIQUE_PTR_WRAPPER(SurfacePtr, SDL_Surface, SDL_FreeSurface)
UNIQUE_PTR_WRAPPER(TexturePtr, SDL_Texture, SDL_DestroyTexture)

// null for failure: error reason printed
RendererPtr create_renderer(const WindowPtr& w) {
  // wsl nividia driver issue:
  // export LIBGL_ALWAYS_SOFTWARE=true
  RendererPtr ret(SDL_CreateRenderer(w.get(), -1, SDL_RENDERER_ACCELERATED | SDL_RENDERER_PRESENTVSYNC));
  if (!ret) {
    fprintf(stderr, "err sdl renderer init: %s", SDL_GetError());
  }
  return ret;
}

class SDLContext {
  SDLContext() {}
  SDLContext(const SDLContext&) = delete;
  SDLContext& operator=(const SDLContext&) = delete;
  SDLContext& operator=(SDLContext&&) = delete;

  bool owner = true;

 public:
  SDLContext(SDLContext&& other) {
    other.owner = false;
  }

  // singleton instance allowed. library cleaned up on dtor
  // null for failure: error reason printed
  static std::optional<SDLContext> create() {
    if (SDL_Init(SDL_INIT_VIDEO) != 0) {
      fprintf(stderr, "err sdl vid init: %s\n", SDL_GetError());
      return {};
    }

    // unicode SDL events for input (init after video init)
    SDL_StartTextInput();

    if (TTF_Init() != 0) {
      fprintf(stderr, "err sdl ttf init: %s\n", TTF_GetError());
      return {};
    }

    return SDLContext();
  }

  // NULL on failure (error printed).
  WindowPtr create_window(const char* title,              //
                          int x = SDL_WINDOWPOS_CENTERED, //
                          int y = SDL_WINDOWPOS_CENTERED, //
                          int width = SCREEN_WIDTH,       //
                          int height = SCREEN_HEIGHT) const {
    WindowPtr ret(SDL_CreateWindow( //
        title, x, y, width, height, //
        SDL_WINDOW_SHOWN | SDL_WINDOW_RESIZABLE | SDL_WINDOW_INPUT_FOCUS));
    if (!ret) {
      fprintf(stderr, "err create window: %s\n", SDL_GetError());
    }
    return ret;
  }

  // null on failure (prints error)
  FontPtr create_font(const char* ttf_path, int size = 28) const {
    FontPtr font = FontPtr(TTF_OpenFont(ttf_path, size));
    if (!font) {
      fprintf(stderr, "err sdl ttf font init: %s\n", TTF_GetError());
      return NULL;
    }
    return font;
  }

  ~SDLContext() {
    // safe to call even if init failed
    if (this->owner) {
      TTF_Quit();
      SDL_StopTextInput();
      SDL_Quit();
    }
  }
};

// a utf8 character has a maximum size of 4 bytes.
static constexpr unsigned int MAX_BYTES_PER_CHARACTER = 4;

// associates utf8 chars with textures. caches
class CharacterManager {
  using Key_t = std::array<char, MAX_BYTES_PER_CHARACTER>;

  struct KeyHasher {
    size_t operator()(const Key_t& k) const {
      auto view = std::string_view(k.cbegin(), k.size());
      return std::hash<std::string_view>{}(view);
    }
  };

  FontPtr font;
  // utf8 can encode a character with a maximum of 4 bytes
  std::unordered_map<Key_t, TexturePtr, KeyHasher> textures;

 public:
  // search for a monospace font and use that as the default
  static std::optional<CharacterManager> create(const SDLContext& ctx) {
    FontPtr mono_font = ctx.create_font(get_mono_ttf().get());
    if (!mono_font) {
      return {};
    }
    CharacterManager lm(std::move(mono_font));
    return lm;
  }

  CharacterManager(FontPtr font) : font(std::move(font)) {}

  // pointer depends on the lifetime of this instance.
  // null on failure (error printed)
  SDL_Texture* get(const Key_t& utf8_char, const RendererPtr& renderer) {
    auto it = textures.find(utf8_char);
    if (it != textures.cend()) {
      // texure has already been renderer
      return it->second.get();
    } else {
      // texture must be generated and inserted

      // make null terminating for pass to TTF render function call
      std::array<char, MAX_BYTES_PER_CHARACTER + 1> null_term_single_utf8_char;
      for (int i = 0; i < MAX_BYTES_PER_CHARACTER; ++i) {
        null_term_single_utf8_char[i] = utf8_char[i];
      }
      null_term_single_utf8_char[MAX_BYTES_PER_CHARACTER] = '\0';

      // create the surface for the character
      auto surface = SurfacePtr(TTF_RenderUTF8_Solid(this->font.get(),                     //
                                                  &*null_term_single_utf8_char.rbegin(), //
                                                  SDL_Color{255, 255, 255}));
      if (!surface) {
        fprintf(stderr, "err sdl ttf font render to surface: %s\n", TTF_GetError());
        return NULL;
      }

      // convert the surface to a texture on gpu
      auto texture = TexturePtr(SDL_CreateTextureFromSurface(renderer.get(), surface.get()));
      if (!texture) {
        fprintf(stderr, "err sdl ttf font surface to texture: %s\n", TTF_GetError());
        return NULL;
      }

      auto inserted_it = textures.emplace_hint(it, utf8_char, std::move(texture));
      return inserted_it->second.get();
    }
  }
};
