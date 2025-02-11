// Copyright 2013 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "cc/input/scrollbar_animation_controller_thinning.h"

#include "cc/layers/solid_color_scrollbar_layer_impl.h"
#include "cc/test/fake_impl_task_runner_provider.h"
#include "cc/test/fake_layer_tree_host_impl.h"
#include "cc/test/geometry_test_utils.h"
#include "cc/test/test_task_graph_runner.h"
#include "cc/trees/layer_tree_impl.h"
#include "testing/gmock/include/gmock/gmock.h"
#include "testing/gtest/include/gtest/gtest.h"

using testing::AtLeast;
using testing::Mock;
using testing::NiceMock;
using testing::_;

namespace cc {
namespace {

// These constants are hard-coded and should match the values in
// scrollbar_animation_controller_thinning.cc.
const float kIdleThicknessScale = 0.4f;
const float kDefaultMouseMoveDistanceToTriggerAnimation = 25.f;

class MockScrollbarAnimationControllerClient
    : public ScrollbarAnimationControllerClient {
 public:
  explicit MockScrollbarAnimationControllerClient(LayerTreeHostImpl* host_impl)
      : host_impl_(host_impl) {}
  virtual ~MockScrollbarAnimationControllerClient() {}

  void PostDelayedScrollbarAnimationTask(const base::Closure& start_fade,
                                         base::TimeDelta delay) override {
    start_fade_ = start_fade;
    delay_ = delay;
  }
  void SetNeedsRedrawForScrollbarAnimation() override {}
  void SetNeedsAnimateForScrollbarAnimation() override {}
  ScrollbarSet ScrollbarsFor(int scroll_layer_id) const override {
    return host_impl_->ScrollbarsFor(scroll_layer_id);
  }
  MOCK_METHOD0(DidChangeScrollbarVisibility, void());

  base::Closure& start_fade() { return start_fade_; }
  base::TimeDelta& delay() { return delay_; }

 private:
  base::Closure start_fade_;
  base::TimeDelta delay_;
  LayerTreeHostImpl* host_impl_;
};

class ScrollbarAnimationControllerThinningTest : public testing::Test {
 public:
  ScrollbarAnimationControllerThinningTest()
      : host_impl_(&task_runner_provider_, &task_graph_runner_),
        client_(&host_impl_) {}

  void ExpectScrollbarsOpacity(float opacity) {
    EXPECT_FLOAT_EQ(opacity, v_scrollbar_layer_->Opacity());
    EXPECT_FLOAT_EQ(opacity, h_scrollbar_layer_->Opacity());
  }

 protected:
  const base::TimeDelta kDelayBeforeStarting = base::TimeDelta::FromSeconds(2);
  const base::TimeDelta kResizeDelayBeforeStarting =
      base::TimeDelta::FromSeconds(5);
  const base::TimeDelta kFadeDuration = base::TimeDelta::FromSeconds(3);
  const base::TimeDelta kThinningDuration = base::TimeDelta::FromSeconds(2);

  void SetUp() override {
    std::unique_ptr<LayerImpl> scroll_layer =
        LayerImpl::Create(host_impl_.active_tree(), 1);
    std::unique_ptr<LayerImpl> clip =
        LayerImpl::Create(host_impl_.active_tree(), 2);
    clip_layer_ = clip.get();
    scroll_layer->SetScrollClipLayer(clip_layer_->id());
    LayerImpl* scroll_layer_ptr = scroll_layer.get();

    const int kThumbThickness = 10;
    const int kTrackStart = 0;
    const bool kIsLeftSideVerticalScrollbar = false;
    const bool kIsOverlayScrollbar = true;

    std::unique_ptr<SolidColorScrollbarLayerImpl> h_scrollbar =
        SolidColorScrollbarLayerImpl::Create(
            host_impl_.active_tree(), 3, HORIZONTAL, kThumbThickness,
            kTrackStart, kIsLeftSideVerticalScrollbar, kIsOverlayScrollbar);
    std::unique_ptr<SolidColorScrollbarLayerImpl> v_scrollbar =
        SolidColorScrollbarLayerImpl::Create(
            host_impl_.active_tree(), 4, VERTICAL, kThumbThickness, kTrackStart,
            kIsLeftSideVerticalScrollbar, kIsOverlayScrollbar);
    v_scrollbar_layer_ = v_scrollbar.get();
    h_scrollbar_layer_ = h_scrollbar.get();

    scroll_layer->test_properties()->AddChild(std::move(v_scrollbar));
    scroll_layer->test_properties()->AddChild(std::move(h_scrollbar));
    clip_layer_->test_properties()->AddChild(std::move(scroll_layer));
    host_impl_.active_tree()->SetRootLayerForTesting(std::move(clip));

    v_scrollbar_layer_->SetScrollLayerId(scroll_layer_ptr->id());
    h_scrollbar_layer_->SetScrollLayerId(scroll_layer_ptr->id());
    v_scrollbar_layer_->test_properties()->opacity_can_animate = true;
    h_scrollbar_layer_->test_properties()->opacity_can_animate = true;
    clip_layer_->SetBounds(gfx::Size(100, 100));
    scroll_layer_ptr->SetBounds(gfx::Size(200, 200));
    host_impl_.active_tree()->BuildLayerListAndPropertyTreesForTesting();

    scrollbar_controller_ = ScrollbarAnimationControllerThinning::Create(
        scroll_layer_ptr->id(), &client_, kDelayBeforeStarting,
        kResizeDelayBeforeStarting, kFadeDuration, kThinningDuration);
  }

  FakeImplTaskRunnerProvider task_runner_provider_;
  TestTaskGraphRunner task_graph_runner_;
  FakeLayerTreeHostImpl host_impl_;
  std::unique_ptr<ScrollbarAnimationControllerThinning> scrollbar_controller_;
  LayerImpl* clip_layer_;
  SolidColorScrollbarLayerImpl* v_scrollbar_layer_;
  SolidColorScrollbarLayerImpl* h_scrollbar_layer_;
  NiceMock<MockScrollbarAnimationControllerClient> client_;
};

// Check initialization of scrollbar. Should start off invisible and thin.
TEST_F(ScrollbarAnimationControllerThinningTest, Idle) {
  ExpectScrollbarsOpacity(0);
  EXPECT_TRUE(scrollbar_controller_->ScrollbarsHidden());
  EXPECT_FLOAT_EQ(kIdleThicknessScale,
                  v_scrollbar_layer_->thumb_thickness_scale_factor());
  EXPECT_FLOAT_EQ(kIdleThicknessScale,
                  v_scrollbar_layer_->thumb_thickness_scale_factor());
}

// Check that scrollbar appears again when the layer becomes scrollable.
TEST_F(ScrollbarAnimationControllerThinningTest, AppearOnResize) {
  base::TimeTicks time;
  time += base::TimeDelta::FromSeconds(1);

  scrollbar_controller_->DidScrollBegin();
  scrollbar_controller_->DidScrollUpdate(false);
  scrollbar_controller_->DidScrollEnd();
  ExpectScrollbarsOpacity(1);

  // Make the Layer non-scrollable, scrollbar disappears.
  clip_layer_->SetBounds(gfx::Size(200, 200));
  scrollbar_controller_->DidScrollUpdate(false);
  ExpectScrollbarsOpacity(0);

  // Make the layer scrollable, scrollbar appears again.
  clip_layer_->SetBounds(gfx::Size(100, 100));
  scrollbar_controller_->DidScrollUpdate(false);
  ExpectScrollbarsOpacity(1);
}

// Check that scrollbar disappears when the layer becomes non-scrollable.
TEST_F(ScrollbarAnimationControllerThinningTest, HideOnResize) {
  base::TimeTicks time;
  time += base::TimeDelta::FromSeconds(1);

  LayerImpl* scroll_layer = host_impl_.active_tree()->LayerById(1);
  ASSERT_TRUE(scroll_layer);
  EXPECT_EQ(gfx::Size(200, 200), scroll_layer->bounds());

  // Shrink along X axis, horizontal scrollbar should appear.
  clip_layer_->SetBounds(gfx::Size(100, 200));
  EXPECT_EQ(gfx::Size(100, 200), clip_layer_->bounds());

  scrollbar_controller_->DidScrollBegin();

  scrollbar_controller_->DidScrollUpdate(false);
  EXPECT_FLOAT_EQ(1, h_scrollbar_layer_->Opacity());

  scrollbar_controller_->DidScrollEnd();

  // Shrink along Y axis and expand along X, horizontal scrollbar
  // should disappear.
  clip_layer_->SetBounds(gfx::Size(200, 100));
  EXPECT_EQ(gfx::Size(200, 100), clip_layer_->bounds());

  scrollbar_controller_->DidScrollBegin();

  scrollbar_controller_->DidScrollUpdate(false);
  EXPECT_FLOAT_EQ(0.0f, h_scrollbar_layer_->Opacity());

  scrollbar_controller_->DidScrollEnd();
}

// Scroll content. Confirm the scrollbar appears and fades out.
TEST_F(ScrollbarAnimationControllerThinningTest, BasicAppearAndFadeOut) {
  base::TimeTicks time;
  time += base::TimeDelta::FromSeconds(1);

  // Scrollbar should be invisible.
  ExpectScrollbarsOpacity(0);
  EXPECT_TRUE(scrollbar_controller_->ScrollbarsHidden());

  // Scrollbar should appear only on scroll update.
  scrollbar_controller_->DidScrollBegin();
  ExpectScrollbarsOpacity(0);
  EXPECT_TRUE(scrollbar_controller_->ScrollbarsHidden());

  scrollbar_controller_->DidScrollUpdate(false);
  ExpectScrollbarsOpacity(1);
  EXPECT_FALSE(scrollbar_controller_->ScrollbarsHidden());

  scrollbar_controller_->DidScrollEnd();
  ExpectScrollbarsOpacity(1);
  EXPECT_FALSE(scrollbar_controller_->ScrollbarsHidden());

  // An animation should have been enqueued.
  EXPECT_EQ(kDelayBeforeStarting, client_.delay());
  EXPECT_FALSE(client_.start_fade().is_null());
  client_.start_fade().Run();

  // Scrollbar should fade out over kFadeDuration.
  scrollbar_controller_->Animate(time);
  time += kFadeDuration;
  scrollbar_controller_->Animate(time);

  ExpectScrollbarsOpacity(0);
  EXPECT_TRUE(scrollbar_controller_->ScrollbarsHidden());
}

// Scroll content. Move the mouse near the scrollbar and confirm it becomes
// thick. Ensure it remains visible as long as the mouse is near the scrollbar.
TEST_F(ScrollbarAnimationControllerThinningTest, MoveNearAndDontFadeOut) {
  base::TimeTicks time;
  time += base::TimeDelta::FromSeconds(1);

  scrollbar_controller_->DidScrollBegin();
  scrollbar_controller_->DidScrollUpdate(false);
  scrollbar_controller_->DidScrollEnd();

  // An animation should have been enqueued.
  EXPECT_EQ(kDelayBeforeStarting, client_.delay());
  EXPECT_FALSE(client_.start_fade().is_null());
  EXPECT_FALSE(client_.start_fade().IsCancelled());

  // Now move the mouse near the scrollbar. This should cancel the currently
  // queued fading animation and start animating thickness.
  scrollbar_controller_->DidMouseMoveNear(VERTICAL, 1);
  ExpectScrollbarsOpacity(1);
  EXPECT_FLOAT_EQ(kIdleThicknessScale,
                  v_scrollbar_layer_->thumb_thickness_scale_factor());
  EXPECT_FLOAT_EQ(kIdleThicknessScale,
                  h_scrollbar_layer_->thumb_thickness_scale_factor());
  EXPECT_TRUE(client_.start_fade().IsCancelled());

  // Vertical scrollbar should become thick.
  scrollbar_controller_->Animate(time);
  time += kThinningDuration;
  scrollbar_controller_->Animate(time);
  ExpectScrollbarsOpacity(1);
  EXPECT_FLOAT_EQ(1, v_scrollbar_layer_->thumb_thickness_scale_factor());
  EXPECT_FLOAT_EQ(kIdleThicknessScale,
                  h_scrollbar_layer_->thumb_thickness_scale_factor());

  // Mouse is still near the Scrollbar. Once the thickness animation is
  // complete, the queued delayed fade animation should be either cancelled or
  // null.
  EXPECT_TRUE(client_.start_fade().is_null() ||
              client_.start_fade().IsCancelled());
}

// Scroll content. Move the mouse over the scrollbar and confirm it becomes
// thick. Ensure it remains visible as long as the mouse is over the scrollbar.
TEST_F(ScrollbarAnimationControllerThinningTest, MoveOverAndDontFadeOut) {
  base::TimeTicks time;
  time += base::TimeDelta::FromSeconds(1);

  scrollbar_controller_->DidScrollBegin();
  scrollbar_controller_->DidScrollUpdate(false);
  scrollbar_controller_->DidScrollEnd();

  // An animation should have been enqueued.
  EXPECT_EQ(kDelayBeforeStarting, client_.delay());
  EXPECT_FALSE(client_.start_fade().is_null());
  EXPECT_FALSE(client_.start_fade().IsCancelled());

  // Now move the mouse over the scrollbar. This should cancel the currently
  // queued fading animation and start animating thickness.
  scrollbar_controller_->DidMouseMoveNear(VERTICAL, 0);
  ExpectScrollbarsOpacity(1);
  EXPECT_FLOAT_EQ(kIdleThicknessScale,
                  v_scrollbar_layer_->thumb_thickness_scale_factor());
  EXPECT_FLOAT_EQ(kIdleThicknessScale,
                  h_scrollbar_layer_->thumb_thickness_scale_factor());
  EXPECT_TRUE(client_.start_fade().IsCancelled());

  // Vertical scrollbar should become thick.
  scrollbar_controller_->Animate(time);
  time += kThinningDuration;
  scrollbar_controller_->Animate(time);
  ExpectScrollbarsOpacity(1);
  EXPECT_FLOAT_EQ(1, v_scrollbar_layer_->thumb_thickness_scale_factor());
  EXPECT_FLOAT_EQ(kIdleThicknessScale,
                  h_scrollbar_layer_->thumb_thickness_scale_factor());

  // Mouse is still over the Scrollbar. Once the thickness animation is
  // complete, the queued delayed fade animation should be either cancelled or
  // null.
  EXPECT_TRUE(client_.start_fade().is_null() ||
              client_.start_fade().IsCancelled());
}

// Make sure a scrollbar captured before the thickening animation doesn't try
// to fade out.
TEST_F(ScrollbarAnimationControllerThinningTest,
       DontFadeWhileCapturedBeforeThick) {
  base::TimeTicks time;
  time += base::TimeDelta::FromSeconds(1);

  scrollbar_controller_->DidScrollBegin();
  scrollbar_controller_->DidScrollUpdate(false);
  scrollbar_controller_->DidScrollEnd();

  // An animation should have been enqueued.
  EXPECT_EQ(kDelayBeforeStarting, client_.delay());
  EXPECT_FALSE(client_.start_fade().is_null());

  // Now move the mouse over the scrollbar and capture it. It should become
  // thick without need for an animation.
  scrollbar_controller_->DidMouseMoveNear(VERTICAL, 0);
  scrollbar_controller_->DidMouseDown();
  ExpectScrollbarsOpacity(1);
  EXPECT_FLOAT_EQ(1, v_scrollbar_layer_->thumb_thickness_scale_factor());
  EXPECT_FLOAT_EQ(kIdleThicknessScale,
                  h_scrollbar_layer_->thumb_thickness_scale_factor());

  // The fade animation should have been cleared or cancelled.
  EXPECT_TRUE(client_.start_fade().is_null() ||
              client_.start_fade().IsCancelled());
}

// Make sure a scrollbar captured then move mouse away doesn't try to fade out.
TEST_F(ScrollbarAnimationControllerThinningTest,
       DontFadeWhileCapturedThenAway) {
  base::TimeTicks time;
  time += base::TimeDelta::FromSeconds(1);

  scrollbar_controller_->DidScrollBegin();
  scrollbar_controller_->DidScrollUpdate(false);
  scrollbar_controller_->DidScrollEnd();

  // An animation should have been enqueued.
  EXPECT_EQ(kDelayBeforeStarting, client_.delay());
  EXPECT_FALSE(client_.start_fade().is_null());

  // Now move the mouse over the scrollbar and capture it. It should become
  // thick without need for an animation.
  scrollbar_controller_->DidMouseMoveNear(VERTICAL, 0);
  scrollbar_controller_->DidMouseDown();
  ExpectScrollbarsOpacity(1);
  EXPECT_FLOAT_EQ(1, v_scrollbar_layer_->thumb_thickness_scale_factor());
  EXPECT_FLOAT_EQ(kIdleThicknessScale,
                  h_scrollbar_layer_->thumb_thickness_scale_factor());

  // The fade animation should have been cleared or cancelled.
  EXPECT_TRUE(client_.start_fade().is_null() ||
              client_.start_fade().IsCancelled());

  // Then move mouse away, The fade animation should have been cleared or
  // cancelled.
  scrollbar_controller_->DidMouseMoveNear(
      VERTICAL, kDefaultMouseMoveDistanceToTriggerAnimation);

  EXPECT_TRUE(client_.start_fade().is_null() ||
              client_.start_fade().IsCancelled());
}

// Make sure a scrollbar captured after a thickening animation doesn't try to
// fade out.
TEST_F(ScrollbarAnimationControllerThinningTest, DontFadeWhileCaptured) {
  base::TimeTicks time;
  time += base::TimeDelta::FromSeconds(1);

  scrollbar_controller_->DidScrollBegin();
  scrollbar_controller_->DidScrollUpdate(false);
  scrollbar_controller_->DidScrollEnd();

  // An animation should have been enqueued.
  EXPECT_EQ(kDelayBeforeStarting, client_.delay());
  EXPECT_FALSE(client_.start_fade().is_null());
  EXPECT_FALSE(client_.start_fade().IsCancelled());

  // Now move the mouse over the scrollbar and animate it until it's thick.
  scrollbar_controller_->DidMouseMoveNear(VERTICAL, 0);
  scrollbar_controller_->Animate(time);
  time += kThinningDuration;
  scrollbar_controller_->Animate(time);
  ExpectScrollbarsOpacity(1);
  EXPECT_FLOAT_EQ(1, v_scrollbar_layer_->thumb_thickness_scale_factor());
  EXPECT_FLOAT_EQ(kIdleThicknessScale,
                  h_scrollbar_layer_->thumb_thickness_scale_factor());

  // Since the mouse is over the scrollbar, it should either clear or cancel the
  // queued fade.
  EXPECT_TRUE(client_.start_fade().is_null() ||
              client_.start_fade().IsCancelled());

  // Make sure the queued fade animation is still null or cancelled after
  // capturing the scrollbar.
  scrollbar_controller_->DidMouseDown();
  EXPECT_TRUE(client_.start_fade().is_null() ||
              client_.start_fade().IsCancelled());
}

// Make sure releasing a captured scrollbar when the mouse isn't near it, causes
// the scrollbar to fade out.
TEST_F(ScrollbarAnimationControllerThinningTest, FadeAfterReleasedFar) {
  base::TimeTicks time;
  time += base::TimeDelta::FromSeconds(1);

  scrollbar_controller_->DidScrollBegin();
  scrollbar_controller_->DidScrollUpdate(false);
  scrollbar_controller_->DidScrollEnd();

  // An animation should have been enqueued.
  EXPECT_EQ(kDelayBeforeStarting, client_.delay());
  EXPECT_FALSE(client_.start_fade().is_null());
  EXPECT_FALSE(client_.start_fade().IsCancelled());

  // Now move the mouse over the scrollbar and capture it.
  scrollbar_controller_->DidMouseMoveNear(VERTICAL, 0);
  scrollbar_controller_->DidMouseDown();
  ExpectScrollbarsOpacity(1);
  EXPECT_FLOAT_EQ(1, v_scrollbar_layer_->thumb_thickness_scale_factor());
  EXPECT_FLOAT_EQ(kIdleThicknessScale,
                  h_scrollbar_layer_->thumb_thickness_scale_factor());

  // Since the mouse is still near the scrollbar, the queued fade should be
  // either null or cancelled.
  EXPECT_TRUE(client_.start_fade().is_null() ||
              client_.start_fade().IsCancelled());

  // Now move the mouse away from the scrollbar and release it.
  scrollbar_controller_->DidMouseMoveNear(
      VERTICAL, kDefaultMouseMoveDistanceToTriggerAnimation);
  scrollbar_controller_->DidMouseUp();

  scrollbar_controller_->Animate(time);
  ExpectScrollbarsOpacity(1);
  EXPECT_FLOAT_EQ(1, v_scrollbar_layer_->thumb_thickness_scale_factor());
  EXPECT_FLOAT_EQ(kIdleThicknessScale,
                  h_scrollbar_layer_->thumb_thickness_scale_factor());
  time += kThinningDuration;
  scrollbar_controller_->Animate(time);
  ExpectScrollbarsOpacity(1);
  EXPECT_FLOAT_EQ(kIdleThicknessScale,
                  v_scrollbar_layer_->thumb_thickness_scale_factor());
  EXPECT_FLOAT_EQ(kIdleThicknessScale,
                  h_scrollbar_layer_->thumb_thickness_scale_factor());

  // The thickness animation is complete, a fade out must be queued.
  EXPECT_FALSE(client_.start_fade().is_null());
  EXPECT_FALSE(client_.start_fade().IsCancelled());
}

// Make sure releasing a captured scrollbar when the mouse is near/over it,
// doesn't cause the scrollbar to fade out.
TEST_F(ScrollbarAnimationControllerThinningTest, DontFadeAfterReleasedNear) {
  base::TimeTicks time;
  time += base::TimeDelta::FromSeconds(1);

  scrollbar_controller_->DidScrollBegin();
  scrollbar_controller_->DidScrollUpdate(false);
  scrollbar_controller_->DidScrollEnd();

  // An animation should have been enqueued.
  EXPECT_EQ(kDelayBeforeStarting, client_.delay());
  EXPECT_FALSE(client_.start_fade().is_null());
  EXPECT_FALSE(client_.start_fade().IsCancelled());

  // Now move the mouse over the scrollbar and capture it.
  scrollbar_controller_->DidMouseMoveNear(VERTICAL, 0);
  scrollbar_controller_->DidMouseDown();
  ExpectScrollbarsOpacity(1);
  EXPECT_FLOAT_EQ(1, v_scrollbar_layer_->thumb_thickness_scale_factor());
  EXPECT_FLOAT_EQ(kIdleThicknessScale,
                  h_scrollbar_layer_->thumb_thickness_scale_factor());

  // Since the mouse is over the scrollbar, the queued fade must be either
  // null or cancelled.
  EXPECT_TRUE(client_.start_fade().is_null() ||
              client_.start_fade().IsCancelled());

  // Mouse is still near the scrollbar, releasing it shouldn't do anything.
  scrollbar_controller_->DidMouseUp();
  EXPECT_TRUE(client_.start_fade().is_null() ||
              client_.start_fade().IsCancelled());
  ExpectScrollbarsOpacity(1);
  EXPECT_FLOAT_EQ(1, v_scrollbar_layer_->thumb_thickness_scale_factor());
  EXPECT_FLOAT_EQ(kIdleThicknessScale,
                  h_scrollbar_layer_->thumb_thickness_scale_factor());
}

// Make sure moving near a scrollbar while it's fading out causes it to reset
// the opacity and thicken.
TEST_F(ScrollbarAnimationControllerThinningTest, MoveNearScrollbarWhileFading) {
  base::TimeTicks time;
  time += base::TimeDelta::FromSeconds(1);

  scrollbar_controller_->DidScrollBegin();
  scrollbar_controller_->DidScrollUpdate(false);
  scrollbar_controller_->DidScrollEnd();

  // A fade animation should have been enqueued. Start it.
  EXPECT_EQ(kDelayBeforeStarting, client_.delay());
  EXPECT_FALSE(client_.start_fade().is_null());
  client_.start_fade().Run();

  scrollbar_controller_->Animate(time);
  ExpectScrollbarsOpacity(1);

  // Proceed half way through the fade out animation.
  time += kFadeDuration / 2;
  scrollbar_controller_->Animate(time);
  ExpectScrollbarsOpacity(.5f);

  // Now move the mouse near the scrollbar. It should reset opacity to 1
  // instantly and start animating to thick.
  scrollbar_controller_->DidMouseMoveNear(VERTICAL, 1);
  ExpectScrollbarsOpacity(1);
  EXPECT_FLOAT_EQ(kIdleThicknessScale,
                  v_scrollbar_layer_->thumb_thickness_scale_factor());
  EXPECT_FLOAT_EQ(kIdleThicknessScale,
                  h_scrollbar_layer_->thumb_thickness_scale_factor());

  scrollbar_controller_->Animate(time);
  time += kThinningDuration;
  scrollbar_controller_->Animate(time);
  ExpectScrollbarsOpacity(1);
  EXPECT_FLOAT_EQ(1, v_scrollbar_layer_->thumb_thickness_scale_factor());
  EXPECT_FLOAT_EQ(kIdleThicknessScale,
                  h_scrollbar_layer_->thumb_thickness_scale_factor());
}

// Make sure we can't capture scrollbar that's completely faded out.
TEST_F(ScrollbarAnimationControllerThinningTest, TestCantCaptureWhenFaded) {
  base::TimeTicks time;
  time += base::TimeDelta::FromSeconds(1);

  scrollbar_controller_->DidScrollBegin();
  scrollbar_controller_->DidScrollUpdate(false);
  scrollbar_controller_->DidScrollEnd();

  EXPECT_EQ(kDelayBeforeStarting, client_.delay());
  EXPECT_FALSE(client_.start_fade().is_null());
  EXPECT_FALSE(client_.start_fade().IsCancelled());
  client_.start_fade().Run();
  scrollbar_controller_->Animate(time);
  ExpectScrollbarsOpacity(1);

  // Fade the scrollbar out completely.
  time += kFadeDuration;
  scrollbar_controller_->Animate(time);
  ExpectScrollbarsOpacity(0);

  // Move mouse over the scrollbar. It shouldn't thicken the scrollbar since
  // it's completely faded out.
  scrollbar_controller_->DidMouseMoveNear(VERTICAL, 0);
  scrollbar_controller_->Animate(time);
  time += kThinningDuration;
  scrollbar_controller_->Animate(time);
  ExpectScrollbarsOpacity(0);
  EXPECT_FLOAT_EQ(1, v_scrollbar_layer_->thumb_thickness_scale_factor());
  EXPECT_FLOAT_EQ(kIdleThicknessScale,
                  h_scrollbar_layer_->thumb_thickness_scale_factor());

  client_.start_fade().Reset();

  // Now try to capture the scrollbar. It shouldn't do anything since it's
  // completely faded out.
  scrollbar_controller_->DidMouseDown();
  ExpectScrollbarsOpacity(0);
  EXPECT_FLOAT_EQ(1, v_scrollbar_layer_->thumb_thickness_scale_factor());
  EXPECT_FLOAT_EQ(kIdleThicknessScale,
                  h_scrollbar_layer_->thumb_thickness_scale_factor());
  EXPECT_TRUE(client_.start_fade().is_null());

  // Similarly, releasing the scrollbar should have no effect.
  scrollbar_controller_->DidMouseUp();
  ExpectScrollbarsOpacity(0);
  EXPECT_FLOAT_EQ(1, v_scrollbar_layer_->thumb_thickness_scale_factor());
  EXPECT_FLOAT_EQ(kIdleThicknessScale,
                  h_scrollbar_layer_->thumb_thickness_scale_factor());
  EXPECT_TRUE(client_.start_fade().is_null());
}

// Initiate a scroll when the pointer is already near the scrollbar. It should
// appear thick and remain thick.
TEST_F(ScrollbarAnimationControllerThinningTest, ScrollWithMouseNear) {
  base::TimeTicks time;
  time += base::TimeDelta::FromSeconds(1);

  scrollbar_controller_->DidMouseMoveNear(VERTICAL, 1);
  scrollbar_controller_->Animate(time);
  time += kThinningDuration;

  // Since the scrollbar isn't visible yet (because we haven't scrolled), we
  // shouldn't have applied the thickening.
  scrollbar_controller_->Animate(time);
  EXPECT_FLOAT_EQ(1, v_scrollbar_layer_->thumb_thickness_scale_factor());
  EXPECT_FLOAT_EQ(kIdleThicknessScale,
                  h_scrollbar_layer_->thumb_thickness_scale_factor());

  scrollbar_controller_->DidScrollBegin();
  scrollbar_controller_->DidScrollUpdate(false);

  // Now that we've received a scroll, we should be thick without an animation.
  ExpectScrollbarsOpacity(1);

  // An animation for the fade should be either null or cancelled, since
  // mouse is still near the scrollbar.
  scrollbar_controller_->DidScrollEnd();
  EXPECT_EQ(kDelayBeforeStarting, client_.delay());
  EXPECT_TRUE(client_.start_fade().is_null() ||
              client_.start_fade().IsCancelled());

  scrollbar_controller_->Animate(time);
  ExpectScrollbarsOpacity(1);
  EXPECT_FLOAT_EQ(1, v_scrollbar_layer_->thumb_thickness_scale_factor());
  EXPECT_FLOAT_EQ(kIdleThicknessScale,
                  h_scrollbar_layer_->thumb_thickness_scale_factor());

  // Scrollbar should still be thick and visible.
  time += kFadeDuration;
  scrollbar_controller_->Animate(time);
  ExpectScrollbarsOpacity(1);
  EXPECT_FLOAT_EQ(1, v_scrollbar_layer_->thumb_thickness_scale_factor());
  EXPECT_FLOAT_EQ(kIdleThicknessScale,
                  h_scrollbar_layer_->thumb_thickness_scale_factor());
}

// Tests that main thread scroll updates immediatley queue a fade animation
// without requiring a ScrollEnd.
TEST_F(ScrollbarAnimationControllerThinningTest, MainThreadScrollQueuesFade) {
  ASSERT_TRUE(client_.start_fade().is_null());

  // A ScrollUpdate without a ScrollBegin indicates a main thread scroll update
  // so we should schedule a fade animation without waiting for a ScrollEnd
  // (which will never come).
  scrollbar_controller_->DidScrollUpdate(false);
  EXPECT_FALSE(client_.start_fade().is_null());
  EXPECT_EQ(kDelayBeforeStarting, client_.delay());

  client_.start_fade().Reset();

  // If we got a ScrollBegin, we shouldn't schedule the fade animation until we
  // get a corresponding ScrollEnd.
  scrollbar_controller_->DidScrollBegin();
  scrollbar_controller_->DidScrollUpdate(false);
  EXPECT_TRUE(client_.start_fade().is_null());
  scrollbar_controller_->DidScrollEnd();
  EXPECT_FALSE(client_.start_fade().is_null());
  EXPECT_EQ(kDelayBeforeStarting, client_.delay());
}

// Make sure that if the scroll update is as a result of a resize, we use the
// resize delay time instead of the default one.
TEST_F(ScrollbarAnimationControllerThinningTest, ResizeFadeDuration) {
  ASSERT_TRUE(client_.delay().is_zero());

  scrollbar_controller_->DidScrollUpdate(true);
  EXPECT_FALSE(client_.start_fade().is_null());
  EXPECT_EQ(kResizeDelayBeforeStarting, client_.delay());

  client_.delay() = base::TimeDelta();

  // We should use the gesture delay rather than the resize delay if we're in a
  // gesture scroll, even if the resize param is set.
  scrollbar_controller_->DidScrollBegin();
  scrollbar_controller_->DidScrollUpdate(true);
  scrollbar_controller_->DidScrollEnd();

  EXPECT_FALSE(client_.start_fade().is_null());
  EXPECT_EQ(kDelayBeforeStarting, client_.delay());
}

// Tests that the fade effect is animated.
TEST_F(ScrollbarAnimationControllerThinningTest, FadeAnimated) {
  base::TimeTicks time;
  time += base::TimeDelta::FromSeconds(1);

  // Scroll to make the scrollbars visible.
  scrollbar_controller_->DidScrollBegin();
  scrollbar_controller_->DidScrollUpdate(false);
  scrollbar_controller_->DidScrollEnd();

  // Appearance is instant.
  ExpectScrollbarsOpacity(1);

  // An animation should have been enqueued.
  EXPECT_EQ(kDelayBeforeStarting, client_.delay());
  EXPECT_FALSE(client_.start_fade().is_null());
  client_.start_fade().Run();

  // Test that at half the fade duration time, the opacity is at half.
  scrollbar_controller_->Animate(time);
  ExpectScrollbarsOpacity(1);

  time += kFadeDuration / 2;
  scrollbar_controller_->Animate(time);
  ExpectScrollbarsOpacity(.5f);

  time += kFadeDuration / 2;
  scrollbar_controller_->Animate(time);
  ExpectScrollbarsOpacity(0);
}

// Tests that the controller tells the client when the scrollbars hide/show.
TEST_F(ScrollbarAnimationControllerThinningTest, NotifyChangedVisibility) {
  base::TimeTicks time;
  time += base::TimeDelta::FromSeconds(1);

  EXPECT_CALL(client_, DidChangeScrollbarVisibility()).Times(1);
  // Scroll to make the scrollbars visible.
  scrollbar_controller_->DidScrollBegin();
  scrollbar_controller_->DidScrollUpdate(false);
  EXPECT_FALSE(scrollbar_controller_->ScrollbarsHidden());
  Mock::VerifyAndClearExpectations(&client_);

  scrollbar_controller_->DidScrollEnd();

  // Play out the fade animation. We shouldn't notify that the scrollbars are
  // hidden until the animation is completly over. We can (but don't have to)
  // notify during the animation that the scrollbars are still visible.
  EXPECT_CALL(client_, DidChangeScrollbarVisibility()).Times(0);
  ASSERT_FALSE(client_.start_fade().is_null());
  client_.start_fade().Run();
  scrollbar_controller_->Animate(time);
  time += kFadeDuration / 4;
  EXPECT_FALSE(scrollbar_controller_->ScrollbarsHidden());
  scrollbar_controller_->Animate(time);
  time += kFadeDuration / 4;
  EXPECT_FALSE(scrollbar_controller_->ScrollbarsHidden());
  scrollbar_controller_->Animate(time);
  time += kFadeDuration / 4;
  EXPECT_FALSE(scrollbar_controller_->ScrollbarsHidden());
  scrollbar_controller_->Animate(time);
  ExpectScrollbarsOpacity(.25f);
  Mock::VerifyAndClearExpectations(&client_);

  EXPECT_CALL(client_, DidChangeScrollbarVisibility()).Times(1);
  time += kFadeDuration / 4;
  scrollbar_controller_->Animate(time);
  EXPECT_TRUE(scrollbar_controller_->ScrollbarsHidden());
  ExpectScrollbarsOpacity(0);
  Mock::VerifyAndClearExpectations(&client_);

  // Calling DidScrollUpdate without a begin (i.e. update from commit) should
  // also notify.
  EXPECT_CALL(client_, DidChangeScrollbarVisibility()).Times(1);
  scrollbar_controller_->DidScrollUpdate(false);
  EXPECT_FALSE(scrollbar_controller_->ScrollbarsHidden());
  Mock::VerifyAndClearExpectations(&client_);
}

// Move the pointer near each scrollbar. Confirm it gets thick and narrow when
// moved away.
TEST_F(ScrollbarAnimationControllerThinningTest, MouseNearEach) {
  base::TimeTicks time;
  time += base::TimeDelta::FromSeconds(1);

  // Scroll to make the scrollbars visible.
  scrollbar_controller_->DidScrollBegin();
  scrollbar_controller_->DidScrollUpdate(false);
  scrollbar_controller_->DidScrollEnd();

  // Near vertical scrollbar
  scrollbar_controller_->DidMouseMoveNear(VERTICAL, 1);
  scrollbar_controller_->Animate(time);
  ExpectScrollbarsOpacity(1);
  EXPECT_FLOAT_EQ(kIdleThicknessScale,
                  v_scrollbar_layer_->thumb_thickness_scale_factor());
  EXPECT_FLOAT_EQ(kIdleThicknessScale,
                  h_scrollbar_layer_->thumb_thickness_scale_factor());

  // Should animate to thickened.
  time += kThinningDuration;
  scrollbar_controller_->Animate(time);
  ExpectScrollbarsOpacity(1);
  EXPECT_FLOAT_EQ(1, v_scrollbar_layer_->thumb_thickness_scale_factor());
  EXPECT_FLOAT_EQ(kIdleThicknessScale,
                  h_scrollbar_layer_->thumb_thickness_scale_factor());

  // Subsequent moves within the nearness threshold should not change anything.
  scrollbar_controller_->DidMouseMoveNear(VERTICAL, 2);
  scrollbar_controller_->Animate(time);
  time += base::TimeDelta::FromSeconds(10);
  scrollbar_controller_->Animate(time);
  ExpectScrollbarsOpacity(1);
  EXPECT_FLOAT_EQ(1, v_scrollbar_layer_->thumb_thickness_scale_factor());
  EXPECT_FLOAT_EQ(kIdleThicknessScale,
                  h_scrollbar_layer_->thumb_thickness_scale_factor());

  // Now move away from bar.
  scrollbar_controller_->DidMouseMoveNear(
      VERTICAL, kDefaultMouseMoveDistanceToTriggerAnimation);
  scrollbar_controller_->Animate(time);
  time += kThinningDuration;
  scrollbar_controller_->Animate(time);
  ExpectScrollbarsOpacity(1);
  EXPECT_FLOAT_EQ(kIdleThicknessScale,
                  v_scrollbar_layer_->thumb_thickness_scale_factor());
  EXPECT_FLOAT_EQ(kIdleThicknessScale,
                  h_scrollbar_layer_->thumb_thickness_scale_factor());

  // Near horizontal scrollbar
  scrollbar_controller_->DidMouseMoveNear(HORIZONTAL, 2);
  scrollbar_controller_->Animate(time);
  ExpectScrollbarsOpacity(1);
  EXPECT_FLOAT_EQ(kIdleThicknessScale,
                  v_scrollbar_layer_->thumb_thickness_scale_factor());
  EXPECT_FLOAT_EQ(kIdleThicknessScale,
                  h_scrollbar_layer_->thumb_thickness_scale_factor());

  // Should animate to thickened.
  time += kThinningDuration;
  scrollbar_controller_->Animate(time);
  ExpectScrollbarsOpacity(1);
  EXPECT_FLOAT_EQ(kIdleThicknessScale,
                  v_scrollbar_layer_->thumb_thickness_scale_factor());
  EXPECT_FLOAT_EQ(1, h_scrollbar_layer_->thumb_thickness_scale_factor());

  // Subsequent moves within the nearness threshold should not change anything.
  scrollbar_controller_->DidMouseMoveNear(HORIZONTAL, 1);
  scrollbar_controller_->Animate(time);
  time += base::TimeDelta::FromSeconds(10);
  scrollbar_controller_->Animate(time);
  ExpectScrollbarsOpacity(1);
  EXPECT_FLOAT_EQ(kIdleThicknessScale,
                  v_scrollbar_layer_->thumb_thickness_scale_factor());
  EXPECT_FLOAT_EQ(1, h_scrollbar_layer_->thumb_thickness_scale_factor());

  // Now move away from bar.
  scrollbar_controller_->DidMouseMoveNear(
      HORIZONTAL, kDefaultMouseMoveDistanceToTriggerAnimation);
  scrollbar_controller_->Animate(time);
  time += kThinningDuration;
  scrollbar_controller_->Animate(time);
  ExpectScrollbarsOpacity(1);
  EXPECT_FLOAT_EQ(kIdleThicknessScale,
                  v_scrollbar_layer_->thumb_thickness_scale_factor());
  EXPECT_FLOAT_EQ(kIdleThicknessScale,
                  h_scrollbar_layer_->thumb_thickness_scale_factor());

  // An animation should have been enqueued.
  EXPECT_FALSE(client_.start_fade().is_null());
  EXPECT_EQ(kDelayBeforeStarting, client_.delay());
}

// Move mouse near both scrollbars at the same time.
TEST_F(ScrollbarAnimationControllerThinningTest, MouseNearBoth) {
  base::TimeTicks time;
  time += base::TimeDelta::FromSeconds(1);

  // Scroll to make the scrollbars visible.
  scrollbar_controller_->DidScrollBegin();
  scrollbar_controller_->DidScrollUpdate(false);
  scrollbar_controller_->DidScrollEnd();

  // Near both Scrollbar
  scrollbar_controller_->DidMouseMoveNear(VERTICAL, 1);
  scrollbar_controller_->DidMouseMoveNear(HORIZONTAL, 1);
  scrollbar_controller_->Animate(time);
  ExpectScrollbarsOpacity(1);
  EXPECT_FLOAT_EQ(kIdleThicknessScale,
                  v_scrollbar_layer_->thumb_thickness_scale_factor());
  EXPECT_FLOAT_EQ(kIdleThicknessScale,
                  h_scrollbar_layer_->thumb_thickness_scale_factor());

  // Should animate to thickened.
  time += kThinningDuration;
  scrollbar_controller_->Animate(time);
  ExpectScrollbarsOpacity(1);
  EXPECT_FLOAT_EQ(1, v_scrollbar_layer_->thumb_thickness_scale_factor());
  EXPECT_FLOAT_EQ(1, h_scrollbar_layer_->thumb_thickness_scale_factor());
}

// Move mouse from one to the other scrollbar before animation is finished, then
// away before animation finished.
TEST_F(ScrollbarAnimationControllerThinningTest,
       MouseNearOtherBeforeAnimationFinished) {
  base::TimeTicks time;
  time += base::TimeDelta::FromSeconds(1);

  // Scroll to make the scrollbars visible.
  scrollbar_controller_->DidScrollBegin();
  scrollbar_controller_->DidScrollUpdate(false);
  scrollbar_controller_->DidScrollEnd();

  // Near vertical scrollbar.
  scrollbar_controller_->DidMouseMoveNear(VERTICAL, 1);
  scrollbar_controller_->Animate(time);
  ExpectScrollbarsOpacity(1);
  EXPECT_FLOAT_EQ(kIdleThicknessScale,
                  v_scrollbar_layer_->thumb_thickness_scale_factor());
  EXPECT_FLOAT_EQ(kIdleThicknessScale,
                  h_scrollbar_layer_->thumb_thickness_scale_factor());

  // Vertical scrollbar animate to half thickened.
  time += kThinningDuration / 2;
  scrollbar_controller_->Animate(time);
  ExpectScrollbarsOpacity(1);
  EXPECT_FLOAT_EQ(kIdleThicknessScale + (1.0f - kIdleThicknessScale) / 2,
                  v_scrollbar_layer_->thumb_thickness_scale_factor());
  EXPECT_FLOAT_EQ(kIdleThicknessScale,
                  h_scrollbar_layer_->thumb_thickness_scale_factor());

  // Away vertical scrollbar and near horizontal scrollbar.
  scrollbar_controller_->DidMouseMoveNear(
      VERTICAL, kDefaultMouseMoveDistanceToTriggerAnimation);
  scrollbar_controller_->DidMouseMoveNear(HORIZONTAL, 1);
  scrollbar_controller_->Animate(time);

  // Vertical scrollbar animate to thin. horizontal scrollbar animate to
  // thickened.
  time += kThinningDuration;
  scrollbar_controller_->Animate(time);
  ExpectScrollbarsOpacity(1);
  EXPECT_FLOAT_EQ(kIdleThicknessScale,
                  v_scrollbar_layer_->thumb_thickness_scale_factor());
  EXPECT_FLOAT_EQ(1, h_scrollbar_layer_->thumb_thickness_scale_factor());

  // Away horizontal scrollbar.
  scrollbar_controller_->DidMouseMoveNear(
      HORIZONTAL, kDefaultMouseMoveDistanceToTriggerAnimation);
  scrollbar_controller_->Animate(time);

  // Horizontal scrollbar animate to thin.
  time += kThinningDuration;
  scrollbar_controller_->Animate(time);
  ExpectScrollbarsOpacity(1);
  EXPECT_FLOAT_EQ(kIdleThicknessScale,
                  v_scrollbar_layer_->thumb_thickness_scale_factor());
  EXPECT_FLOAT_EQ(kIdleThicknessScale,
                  h_scrollbar_layer_->thumb_thickness_scale_factor());

  // An animation should have been enqueued.
  EXPECT_FALSE(client_.start_fade().is_null());
  EXPECT_EQ(kDelayBeforeStarting, client_.delay());
}

}  // namespace
}  // namespace cc
