#pragma once

#include <fontconfig/fontconfig.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include "mem_utils.hpp"

// 0 for yes, otherwise no
int TTFFileExtension(const char* string) {
  string = strrchr(string, '.');
  if (string != NULL) {
    return strcmp(string, ".ttf");
  }
  return -1;
}

// c string to ttf file, or null (error printed)
UniqMalloc get_ttf(const char* pattern) {
  if (!FcInit()) {
    fputs("err font-config init\n", stderr);
    return NULL;
  }
  scope_exit{FcFini};

  UNIQUE_PTR_WRAPPER(FcPatternPtr, FcPattern, FcPatternDestroy)
  UNIQUE_PTR_WRAPPER(FcObjectSetPtr, FcObjectSet, FcObjectSetDestroy)
  UNIQUE_PTR_WRAPPER(FcFontSetPtr, FcFontSet, FcFontSetDestroy)

  auto fcPattern = FcPatternPtr(FcNameParse(reinterpret_cast<const FcChar8*>(pattern)));
  if (!fcPattern) {
    fputs("err font-config FcNameParse\n", stderr);
    return NULL;
  }

  auto fcObjectSet = FcObjectSetPtr(FcObjectSetBuild(FC_FILE, NULL));
  if (fcObjectSet == NULL) {
    fputs("err font-config FcObjectSetBuild\n", stderr);
    return NULL;
  }

  auto fcFontList = FcFontSetPtr(FcFontList(0, fcPattern.get(), fcObjectSet.get()));

  for (int i = 0; i < fcFontList->nfont; ++i) {
    FcPattern* fontPattern = fcFontList->fonts[i];

    FcChar8* file;
    if (FcPatternGetString(fontPattern, FC_FILE, 0, &file) == FcResultMatch) {
      if (TTFFileExtension(reinterpret_cast<char*>(file)) == 0) {
        // copy then return file
        size_t length = strlen((char*)file);
        size_t byte_length = (length + 1) * sizeof(char);
        void* ret = malloc(byte_length);
        if (!ret) {
          return NULL;
        }
        memcpy(ret, file, byte_length);
        return UniqMalloc(ret);
      }
    }
  }

  fputs("err font-config couldn't find mono ttf\n", stderr);
  return NULL;
}

UniqMalloc get_mono_ttf() {
  return get_ttf(":mono");
}
