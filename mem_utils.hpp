#pragma once

#include <memory>

#define UNIQUE_PTR_WRAPPER(ExportName, Type, DestroyFunction) \
  struct destroyer_from_macro_gen_##ExportName {              \
    void operator()(Type* w) {                                \
      if (w) {                                                \
        DestroyFunction(w);                                   \
      }                                                       \
    }                                                         \
  };                                                          \
  using ExportName = std::unique_ptr<Type, destroyer_from_macro_gen_##ExportName>;

template <typename F>
struct scope_exit {
  F f_;
  ~scope_exit() { f_(); }
};

template <class F>
scope_exit(F) -> scope_exit<F>;


struct destroyer_for_malloc {
  void operator()(void* p) { free(p); } // explicitely no null check
};

using UniqMalloc = std::unique_ptr<void, destroyer_for_malloc>;
