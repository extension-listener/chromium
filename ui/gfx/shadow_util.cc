// Copyright 2017 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ui/gfx/shadow_util.h"

#include <map>
#include <vector>

#include "base/lazy_instance.h"
#include "third_party/skia/include/core/SkDrawLooper.h"
#include "third_party/skia/include/core/SkRRect.h"
#include "ui/gfx/canvas.h"
#include "ui/gfx/geometry/insets.h"
#include "ui/gfx/image/canvas_image_source.h"
#include "ui/gfx/shadow_value.h"
#include "ui/gfx/skia_paint_util.h"
#include "ui/gfx/skia_util.h"

namespace gfx {
namespace {

// Creates an image with the given shadows painted around a round rect with
// the given corner radius. The image will be just large enough to paint the
// shadows appropriately with a 1px square region reserved for "content".
class ShadowNineboxSource : public CanvasImageSource {
 public:
  ShadowNineboxSource(const std::vector<ShadowValue>& shadows,
                      float corner_radius)
      : CanvasImageSource(CalculateSize(shadows, corner_radius), false),
        shadows_(shadows),
        corner_radius_(corner_radius) {
    DCHECK(!shadows.empty());
  }
  ~ShadowNineboxSource() override {}

  // CanvasImageSource overrides:
  void Draw(Canvas* canvas) override {
    SkPaint paint;
    paint.setLooper(CreateShadowDrawLooperCorrectBlur(shadows_));
    Insets insets = -ShadowValue::GetMargin(shadows_);
    gfx::Rect bounds(size());
    bounds.Inset(insets);
    SkRRect r_rect = SkRRect::MakeRectXY(gfx::RectToSkRect(bounds),
                                         corner_radius_, corner_radius_);

    // Clip out the center so it's not painted with the shadow.
    canvas->sk_canvas()->clipRRect(r_rect, SkClipOp::kDifference, true);
    // Clipping alone is not enough --- due to anti aliasing there will still be
    // some of the fill color in the rounded corners. We must make the fill
    // color transparent.
    paint.setColor(SK_ColorTRANSPARENT);
    canvas->sk_canvas()->drawRRect(r_rect, paint);
  }

 private:
  static Size CalculateSize(const std::vector<ShadowValue>& shadows,
                            float corner_radius) {
    // The "content" area (the middle tile in the 3x3 grid) is a single pixel.
    gfx::Rect bounds(0, 0, 1, 1);
    // We need enough space to render the full range of blur.
    bounds.Inset(-ShadowValue::GetBlurRegion(shadows));
    // We also need space for the full roundrect corner rounding.
    bounds.Inset(-gfx::Insets(corner_radius));
    return bounds.size();
  }

  const std::vector<ShadowValue> shadows_;

  const float corner_radius_;

  DISALLOW_COPY_AND_ASSIGN(ShadowNineboxSource);
};

// Map from elevation/corner radius pair to a cached shadow.
using ShadowDetailsMap = std::map<std::pair<int, int>, ShadowDetails>;
base::LazyInstance<ShadowDetailsMap> g_shadow_cache = LAZY_INSTANCE_INITIALIZER;

}  // namespace

ShadowDetails::ShadowDetails() {}
ShadowDetails::ShadowDetails(const ShadowDetails& other) = default;
ShadowDetails::~ShadowDetails() {}

const ShadowDetails& ShadowDetails::Get(int elevation, int corner_radius) {
  auto iter =
      g_shadow_cache.Get().find(std::make_pair(elevation, corner_radius));
  if (iter != g_shadow_cache.Get().end())
    return iter->second;

  auto insertion = g_shadow_cache.Get().insert(std::make_pair(
      std::make_pair(elevation, corner_radius), ShadowDetails()));
  DCHECK(insertion.second);
  ShadowDetails* shadow = &insertion.first->second;
  // To match the CSS notion of blur (spread outside the bounding box) to the
  // Skia notion of blur (spread outside and inside the bounding box), we have
  // to double the designer-provided blur values.
  const int kBlurCorrection = 2;
  // "Key shadow": y offset is elevation and blur is twice the elevation.
  shadow->values.emplace_back(gfx::Vector2d(0, elevation),
                              kBlurCorrection * elevation * 2,
                              SkColorSetA(SK_ColorBLACK, 0x3d));
  // "Ambient shadow": no offset and blur matches the elevation.
  shadow->values.emplace_back(gfx::Vector2d(), kBlurCorrection * elevation,
                              SkColorSetA(SK_ColorBLACK, 0x1f));
  // To see what this looks like for elevation 24, try this CSS:
  //   box-shadow: 0 24px 48px rgba(0, 0, 0, .24),
  //               0 0 24px rgba(0, 0, 0, .12);
  auto source = new ShadowNineboxSource(shadow->values, corner_radius);
  shadow->ninebox_image = ImageSkia(source, source->size());
  return *shadow;
}

}  // namespace gfx
