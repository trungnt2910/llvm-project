//===-FrameRegistry.hpp ------------------------------------------------===//
//
// Part of the LLVM Project, under the Apache License v2.0 with LLVM Exceptions.
// See https://llvm.org/LICENSE.txt for license information.
// SPDX-License-Identifier: Apache-2.0 WITH LLVM-exception
//
// Provides a frame object registry to store a linked list of objects
// pass to __register_frame_info.
//
//===----------------------------------------------------------------------===//

#ifndef __FRAMEREGISTRY_HPP__
#define __FRAMEREGISTRY_HPP__

#include "config.h"
#include <limits.h>

#ifdef _LIBUNWIND_DEBUG_FRAMEREGISTRY
#define _LIBUNWIND_FRAMEREGISTRY_TRACE0(x) _LIBUNWIND_LOG0(x)
#define _LIBUNWIND_FRAMEREGISTRY_TRACE(msg, ...)                            \
  _LIBUNWIND_LOG(msg, __VA_ARGS__)
#else
#define _LIBUNWIND_FRAMEREGISTRY_TRACE0(x)
#define _LIBUNWIND_FRAMEREGISTRY_TRACE(msg, ...)
#endif

class _LIBUNWIND_HIDDEN FrameRegistry {
public:
  typedef uintptr_t pint_t;
  // The memory provided by __register_frame_info
  // It could be anything that fits into an array of 8 void pointers.
  struct object {
    // The object's group (for later use with __deregister_frame_info)
    void *group;
    // The real start of the .eh_frame section.
    void *eh_frame_start;
    object *next;
  };
private:
  object *Head = NULL;
public:
  void add(object *obj) {
    _LIBUNWIND_FRAMEREGISTRY_TRACE("FrameRegistry add: group: %p, eh_frame_start: %p",
                                   obj->group, obj->eh_frame_start);
    if (obj->next != nullptr) {
      _LIBUNWIND_FRAMEREGISTRY_TRACE0("FrameRegistry add: already in list");
      return;
    }
    obj->next = Head;
    Head = obj;
  }
  void remove(pint_t group) {
    _LIBUNWIND_FRAMEREGISTRY_TRACE("FrameRegistry remove: %p", (void *)group);
    object *prev = NULL;
    object *cur = Head;
    while (cur != NULL) {
      if ((pint_t)cur->group == group) {
        if (prev == NULL) {
          Head = cur->next;
        } else {
          prev->next = cur->next;
        }
        cur->next = NULL;
        return;
      }
      prev = cur;
      cur = cur->next;
    }
  }
  object *find(pint_t startAddress, pint_t endAddress) {
    _LIBUNWIND_FRAMEREGISTRY_TRACE("FrameRegistry find: start: %p, end: %p",
                                   (void *)startAddress, (void *)endAddress);
    object *cur = Head;
    while (cur != NULL) {
      if ((pint_t)cur->eh_frame_start >= startAddress &&
          (pint_t)cur->eh_frame_start < endAddress) {
        return cur;
      }
      cur = cur->next;
    }
    return NULL;
  }
};

#endif // __FRAMEREGISTRY_HPP__
