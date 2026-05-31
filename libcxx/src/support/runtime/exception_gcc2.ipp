// -*- C++ -*-
//===----------------------------------------------------------------------===//
//
// Part of the LLVM Project, under the Apache License v2.0 with LLVM Exceptions.
// See https://llvm.org/LICENSE.txt for license information.
// SPDX-License-Identifier: Apache-2.0 WITH LLVM-exception
//
//===----------------------------------------------------------------------===//

#include <__config>
#include <exception>
#include <new>

// GCC 2.95 legacy compiler-runtime Exception Control Header structures
struct __eh_info {
  void *match_function;
  short language;
  short version;
};

struct cp_eh_info {
  __eh_info eh_info;
  void *value;                  // The thrown exception payload pointer
  void *type;                   // The typeinfo pointer
  void (*cleanup)(void *, int); // The registered destructor function
  bool caught;
  cp_eh_info *next;
  long handlers;
  void *original_value;
};

// GCC 2.95 legacy compiler-runtime exception helper
extern "C" {
  cp_eh_info* __cp_eh_info() throw();
}

namespace std {
  // Natively implement C++17 std::uncaught_exceptions() plural count by walking the active stack!
  int uncaught_exceptions() noexcept {
    int count = 0;
    cp_eh_info* p = __cp_eh_info();
    while (p) {
      if (!p->caught) {
        count++;
      }
      p = p->next;
    }
    return count;
  }

  // Minimalist, exact allocation exception class stubs needed by the compiler
  bad_alloc::bad_alloc() noexcept {}
  bad_alloc::~bad_alloc() noexcept {}
  const char* bad_alloc::what() const noexcept { return "std::bad_alloc"; }

  bad_array_new_length::bad_array_new_length() noexcept {}
  bad_array_new_length::~bad_array_new_length() noexcept {}
  const char* bad_array_new_length::what() const noexcept { return "std::bad_array_new_length"; }
}
