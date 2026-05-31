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
#include <__verbose_abort>
#include <cstdio>
#include <cstdint>

// GCC 2.95 legacy compiler-runtime exception helpers (C-linkage symbols)
extern "C" {
  void __eh_free(void*) throw();
  [[noreturn]] void __throw(); // Low-level compiler unwinder
  void** __get_eh_info() throw(); // Returns thread-local double pointer to eh->info
}

// Map conflict-free C++ helper directly to the C++ mangled symbol in libstdc++.a
bool gcc2_is_pointer(void*) __asm__("__is_pointer__FPv");

// Explicit declaration of the compiler's private thread-local exceptions context
struct eh_context {
  void *info;          // Points to the active cp_eh_info block
  void *table_index;   // Active EH table search index
};

namespace std {
  exception_ptr::~exception_ptr() noexcept {
    if (__ptr_) {
      cp_eh_info* p = static_cast<cp_eh_info*>(__ptr_);

      // Natively decrement and deallocate matching GCC 2.95 specifications
      if (--p->handlers == 0) {
        if (p->cleanup) {
          p->cleanup(p->original_value, 2);
        }

        if (!gcc2_is_pointer(p->type)) {
          __eh_free(p->original_value);
        }

        // SAFELY UNLINK p FROM ACTIVE COMPILER STACK LIST IF IT IS STILL PRESENT!
        eh_context* ctx = reinterpret_cast<eh_context*>(__get_eh_info());
        if (ctx) {
          if (ctx->info == p) {
            ctx->info = p->next;
          } else {
            cp_eh_info* prev = static_cast<cp_eh_info*>(ctx->info);
            while (prev && prev->next != p) {
              prev = prev->next;
            }
            if (prev) {
              prev->next = p->next;
            }
          }
        }

        __eh_free(p);
      }
    }
  }

  exception_ptr::exception_ptr(const exception_ptr& other) noexcept : __ptr_(other.__ptr_) {
    if (__ptr_) {
      static_cast<cp_eh_info*>(__ptr_)->handlers++;
    }
  }

  exception_ptr& exception_ptr::operator=(const exception_ptr& other) noexcept {
    if (__ptr_ != other.__ptr_) {
      this->~exception_ptr();
      __ptr_ = other.__ptr_;
      if (__ptr_) {
        static_cast<cp_eh_info*>(__ptr_)->handlers++;
      }
    }
    return *this;
  }

  exception_ptr exception_ptr::__from_native_exception_pointer(void *__e) noexcept {
    exception_ptr p;
    p.__ptr_ = __cp_eh_info();
    if (p.__ptr_) {
      static_cast<cp_eh_info*>(p.__ptr_)->handlers++;
    }
    return p;
  }

  exception_ptr current_exception() noexcept {
    exception_ptr p;
    p.__ptr_ = __cp_eh_info();
    if (p.__ptr_) {
      static_cast<cp_eh_info*>(p.__ptr_)->handlers++;
    }
    return p;
  }

  [[noreturn]] void rethrow_exception(exception_ptr p) {
    if (!p) {
      std::terminate();
    }

    cp_eh_info* eh = static_cast<cp_eh_info*>(p.__ptr_);

    // 1. Type-safely push existing control block onto the thread's active stack
    eh_context* ctx = reinterpret_cast<eh_context*>(__get_eh_info());
    if (ctx) {
      // PREVENT CIRCULAR next LOOPS (eh->next = eh) IF eh IS ALREADY STACK TOP!
      if (ctx->info != eh) {
        eh->next = static_cast<cp_eh_info*>(ctx->info);
        ctx->info = eh;
      }
      
      // 2. CLEAR STALE COMPILER UNWINDER SEARCH STATE TYPE-SAFELY!
      ctx->table_index = nullptr;
    }

    // 3. Raise the low-level unwinder
    __throw();

    std::terminate();
  }
}
