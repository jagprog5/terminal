#pragma once

#include <SDL2/SDL.h>
#include <SDL2/SDL_ttf.h>
#include <stdio.h>
#include <optional>
#include <unordered_map>

#include "font_utils.hpp"
#include "mem_utils.hpp"
#include "string_utils.hpp"

static constexpr unsigned int SINGLE_CHAR_WIDTH = 8;
static constexpr unsigned int SINGLE_CHAR_HEIGHT = 16;
static constexpr unsigned int SCREEN_WIDTH = SINGLE_CHAR_WIDTH * 80;
static constexpr unsigned int FONT_RESOLUTION = 32;
static constexpr unsigned int SCREEN_HEIGHT = SINGLE_CHAR_HEIGHT * 24;

UNIQUE_PTR_WRAPPER(WindowPtr, SDL_Window, SDL_DestroyWindow)
UNIQUE_PTR_WRAPPER(FontPtr, TTF_Font, TTF_CloseFont)
UNIQUE_PTR_WRAPPER(RendererPtr, SDL_Renderer, SDL_DestroyRenderer)
UNIQUE_PTR_WRAPPER(SurfacePtr, SDL_Surface, SDL_FreeSurface)
UNIQUE_PTR_WRAPPER(TexturePtr, SDL_Texture, SDL_DestroyTexture)

// null for failure: error reason printed
RendererPtr create_renderer(const WindowPtr& w) {
  // wsl nividia driver issue for valgrind:
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
  SDLContext(SDLContext&& other) { other.owner = false; }

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

    SDL_SetHint( SDL_HINT_RENDER_SCALE_QUALITY, "2");

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
  FontPtr create_font(const char* ttf_path, int size = FONT_RESOLUTION) const {
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

// associates UTF8Block with textures. caches
class CharacterManager {
  FontPtr font;
  std::unordered_map<UTF8Block, TexturePtr> textures;

 public:
  // search for a monospace font and use that as the default
  static std::optional<CharacterManager> create(const SDLContext& ctx) {
    UniqMalloc um = get_mono_ttf();
    if (!um) {
      return {};
    }
    FontPtr mono_font = ctx.create_font((char*)um.get());
    if (!mono_font) {
      return {};
    }
    CharacterManager cm(std::move(mono_font));
    return std::move(cm);
  }

  CharacterManager(FontPtr font) : font(std::move(font)) {}

  // pointer depends on the lifetime of this instance.
  // null on failure (error printed)
  SDL_Texture* get(UTF8Block utf8_char, const RendererPtr& renderer) {
    auto it = textures.find(utf8_char);
    if (it != textures.cend()) {
      // texure has already been renderer
      return it->second.get();
    } else {
      // texture must be generated and inserted

      // see if the char is valid
      std::optional<wchar_t> maybe_wc = utf8_char.to_wc();
      wchar_t wc;
      if (maybe_wc.has_value()) {
        // it it is, then use it
        wc = *maybe_wc;
      } else {
        // not valid UTF-8. use the character that represents something invalid
        utf8_char = UTF8Block::invalid_utf8();
        wc = *utf8_char.to_wc();
      }

      // give the character in wide char form, is it drawable?
      int has_glyph;
      if (sizeof(wchar_t) == 2) {
        has_glyph = TTF_GlyphIsProvided(this->font.get(), wc);
      } else { // 4 byte. only 2 or 4 is possible
        has_glyph = TTF_GlyphIsProvided32(this->font.get(), wc);
      }

      if (has_glyph == 0) {
        // it's not drawable
        utf8_char = UTF8Block::no_glyph();
      }

      // create the surface for the character
      // render with white, since it can be tinted later with SDL_SetTextureColorMod
      auto surface = SurfacePtr(TTF_RenderUTF8_Blended(this->font.get(), //
                                                       utf8_char.data,   //
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
