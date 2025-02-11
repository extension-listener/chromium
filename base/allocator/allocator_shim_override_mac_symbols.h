// Copyright 2017 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifdef BASE_ALLOCATOR_ALLOCATOR_SHIM_OVERRIDE_MAC_SYMBOLS_H_
#error This header is meant to be included only once by allocator_shim.cc
#endif
#define BASE_ALLOCATOR_ALLOCATOR_SHIM_OVERRIDE_MAC_SYMBOLS_H_

#include "base/allocator/allocator_interception_mac.h"
#include "third_party/apple_apsl/malloc.h"

namespace base {
namespace allocator {

void OverrideMacSymbols() {
  MallocZoneFunctions new_functions;
  new_functions.size = [](malloc_zone_t* zone, const void* ptr) -> size_t {
    return ShimGetSizeEstimate(ptr);
  };
  new_functions.malloc = [](malloc_zone_t* zone, size_t size) -> void* {
    return ShimMalloc(size);
  };
  new_functions.calloc = [](malloc_zone_t* zone, size_t n,
                            size_t size) -> void* {
    return ShimCalloc(n, size);
  };
  new_functions.valloc = [](malloc_zone_t* zone, size_t size) -> void* {
    return ShimValloc(size);
  };
  new_functions.free = [](malloc_zone_t* zone, void* ptr) { ShimFree(ptr); };
  new_functions.realloc = [](malloc_zone_t* zone, void* ptr,
                             size_t size) -> void* {
    return ShimRealloc(ptr, size);
  };
  new_functions.batch_malloc = [](struct _malloc_zone_t* zone, size_t size,
                                  void** results,
                                  unsigned num_requested) -> unsigned {
    return ShimBatchMalloc(size, results, num_requested);
  };
  new_functions.batch_free = [](struct _malloc_zone_t* zone, void** to_be_freed,
                                unsigned num_to_be_freed) -> void {
    ShimBatchFree(to_be_freed, num_to_be_freed);
  };
  new_functions.memalign = [](malloc_zone_t* zone, size_t alignment,
                              size_t size) -> void* {
    return ShimMemalign(alignment, size);
  };
  new_functions.free_definite_size = [](malloc_zone_t* zone, void* ptr,
                                        size_t size) {
    ShimFreeDefiniteSize(ptr, size);
  };

  base::allocator::ReplaceFunctionsForDefaultZone(&new_functions);
}

}  // namespace allocator
}  // namespace base
