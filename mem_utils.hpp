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
