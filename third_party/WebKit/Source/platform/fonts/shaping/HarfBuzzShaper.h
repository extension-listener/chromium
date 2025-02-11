/*
 * Copyright (C) 2012 Google Inc. All rights reserved.
 *
 * Redistribution and use in source and binary forms, with or without
 * modification, are permitted provided that the following conditions are
 * met:
 *
 *     * Redistributions of source code must retain the above copyright
 * notice, this list of conditions and the following disclaimer.
 *     * Redistributions in binary form must reproduce the above
 * copyright notice, this list of conditions and the following disclaimer
 * in the documentation and/or other materials provided with the
 * distribution.
 *     * Neither the name of Google Inc. nor the names of its
 * contributors may be used to endorse or promote products derived from
 * this software without specific prior written permission.
 *
 * THIS SOFTWARE IS PROVIDED BY THE COPYRIGHT HOLDERS AND CONTRIBUTORS
 * "AS IS" AND ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT
 * LIMITED TO, THE IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR
 * A PARTICULAR PURPOSE ARE DISCLAIMED. IN NO EVENT SHALL THE COPYRIGHT
 * OWNER OR CONTRIBUTORS BE LIABLE FOR ANY DIRECT, INDIRECT, INCIDENTAL,
 * SPECIAL, EXEMPLARY, OR CONSEQUENTIAL DAMAGES (INCLUDING, BUT NOT
 * LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS OR SERVICES; LOSS OF USE,
 * DATA, OR PROFITS; OR BUSINESS INTERRUPTION) HOWEVER CAUSED AND ON ANY
 * THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT LIABILITY, OR TORT
 * (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY OUT OF THE USE
 * OF THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF SUCH DAMAGE.
 */

#ifndef HarfBuzzShaper_h
#define HarfBuzzShaper_h

#include "platform/fonts/shaping/RunSegmenter.h"
#include "platform/fonts/shaping/ShapeResult.h"
#include "wtf/Allocator.h"
#include "wtf/Deque.h"
#include "wtf/Vector.h"

namespace blink {

class Font;
class SimpleFontData;
class HarfBuzzShaper;
struct HolesQueueItem;

class PLATFORM_EXPORT HarfBuzzShaper final {
 public:
  HarfBuzzShaper(const UChar*, unsigned length);

  // Shape a range, defined by the start and end parameters, of the string
  // supplied to the constructor.
  // The start and end positions should represent boundaries where a break may
  // occur, such as at the beginning or end of lines or at element boundaries.
  // If given arbitrary positions the results are not guaranteed to be correct.
  // May be called multiple times; font and direction may vary between calls.
  PassRefPtr<ShapeResult> shape(const Font*,
                                TextDirection,
                                unsigned start,
                                unsigned end) const;

  // Shape the entire string with a single font and direction.
  // Equivalent to calling the range version with a start offset of zero and an
  // end offset equal to the length.
  PassRefPtr<ShapeResult> shape(const Font*, TextDirection) const;

  ~HarfBuzzShaper() {}

 private:
  struct RangeData;

  // Shapes a single seqment, as identified by the RunSegmenterRange parameter,
  // one or more times taking font fallback into account. The start and end
  // parameters are for the entire text run, not the segment, and are used to
  // determine pre- and post-context for shaping.
  void shapeSegment(RangeData*,
                    RunSegmenter::RunSegmenterRange,
                    ShapeResult*) const;

  bool extractShapeResults(RangeData*,
                           bool& fontCycleQueued,
                           const HolesQueueItem&,
                           const SimpleFontData*,
                           UScriptCode,
                           bool isLastResort,
                           ShapeResult*) const;

  bool collectFallbackHintChars(const Deque<HolesQueueItem>&,
                                Vector<UChar32>& hint) const;

  const UChar* m_text;
  unsigned m_textLength;
};

}  // namespace blink

#endif  // HarfBuzzShaper_h
