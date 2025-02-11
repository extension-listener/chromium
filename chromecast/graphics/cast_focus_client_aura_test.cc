// Copyright 2017 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chromecast/graphics/cast_focus_client_aura.h"

#include <memory>

#include "ui/aura/test/aura_test_base.h"
#include "ui/aura/test/test_window_delegate.h"
#include "ui/aura/window.h"

namespace chromecast {
namespace test {

using CastFocusClientAuraTest = aura::test::AuraTestBase;

class TestWindow {
 public:
  TestWindow()
      : delegate_(new aura::test::TestWindowDelegate()),
        window_(new aura::Window(delegate_)) {
    window_->Init(ui::LAYER_NOT_DRAWN);
    window_->Show();
  }

  virtual ~TestWindow() {}

  aura::test::TestWindowDelegate* delegate() const { return delegate_; }
  aura::Window* window() const { return window_.get(); }

 private:
  aura::test::TestWindowDelegate* const delegate_;
  std::unique_ptr<aura::Window> const window_;

  DISALLOW_COPY_AND_ASSIGN(TestWindow);
};

TEST_F(CastFocusClientAuraTest, FocusableWindows) {
  std::unique_ptr<aura::WindowTreeHost> window_tree_host(
      aura::WindowTreeHost::Create(gfx::Rect(0, 0, 1280, 720)));
  window_tree_host->InitHost();
  window_tree_host->Show();

  CastFocusClientAura focus_client;

  std::unique_ptr<TestWindow> test_window(new TestWindow);
  window_tree_host->window()->AddChild(test_window->window());

  // Confirm that we can't add an un-focusable window.
  test_window->delegate()->set_can_focus(false);
  focus_client.FocusWindow(test_window->window());
  EXPECT_FALSE(focus_client.GetFocusedWindow());

  // Confirm that we can add a focusable window.
  test_window->delegate()->set_can_focus(true);
  focus_client.FocusWindow(test_window->window());
  EXPECT_EQ(test_window->window(), focus_client.GetFocusedWindow());

  // Confirm that the focused window loses focus when losing visibility.
  test_window->window()->Hide();
  EXPECT_FALSE(focus_client.GetFocusedWindow());

  // Confirm that we find a focusable window when it becomes visible.
  test_window->window()->Show();
  EXPECT_EQ(test_window->window(), focus_client.GetFocusedWindow());

  // Confirm that the focused window loses focus when it is destroyed.
  test_window.reset();
  EXPECT_FALSE(focus_client.GetFocusedWindow());
}

TEST_F(CastFocusClientAuraTest, ChildFocus) {
  std::unique_ptr<aura::WindowTreeHost> window_tree_host(
      aura::WindowTreeHost::Create(gfx::Rect(0, 0, 1280, 720)));
  window_tree_host->InitHost();
  window_tree_host->Show();

  CastFocusClientAura focus_client;

  std::unique_ptr<TestWindow> parent(new TestWindow);
  parent->delegate()->set_can_focus(true);
  window_tree_host->window()->AddChild(parent->window());

  std::unique_ptr<TestWindow> child(new TestWindow);
  child->delegate()->set_can_focus(true);
  parent->window()->AddChild(child->window());

  // Confirm that the child window has the focus, not its top-level parent
  // window.
  focus_client.FocusWindow(child->window());
  EXPECT_EQ(child->window(), focus_client.GetFocusedWindow());

  // Confirm that removing the child window doesn't focus the parent window
  // (since we've never requested focus for the parent window).
  parent->window()->RemoveChild(child->window());
  EXPECT_FALSE(focus_client.GetFocusedWindow());

  // Confirm that we still have no focused window after re-adding the child
  // window, because we haven't requested focus.
  parent->window()->AddChild(child->window());
  EXPECT_FALSE(focus_client.GetFocusedWindow());

  // Request focus and confirm that the child is focused.
  focus_client.FocusWindow(child->window());
  EXPECT_EQ(child->window(), focus_client.GetFocusedWindow());
}

TEST_F(CastFocusClientAuraTest, ZOrder) {
  std::unique_ptr<aura::WindowTreeHost> window_tree_host(
      aura::WindowTreeHost::Create(gfx::Rect(0, 0, 1280, 720)));
  window_tree_host->InitHost();
  window_tree_host->Show();

  CastFocusClientAura focus_client;

  // Add the window with the lowest z-order.
  std::unique_ptr<TestWindow> low(new TestWindow);
  low->delegate()->set_can_focus(true);
  low->window()->set_id(1);
  window_tree_host->window()->AddChild(low->window());
  focus_client.FocusWindow(low->window());
  EXPECT_EQ(low->window(), focus_client.GetFocusedWindow());

  // Add the window with the highest z-order, and confirm that it gets focus.
  std::unique_ptr<TestWindow> high(new TestWindow);
  high->delegate()->set_can_focus(true);
  high->window()->set_id(3);
  window_tree_host->window()->AddChild(high->window());
  focus_client.FocusWindow(high->window());
  EXPECT_EQ(high->window(), focus_client.GetFocusedWindow());

  // Add the window with the middle z-order, and confirm that focus remains with
  // the highest z-order window.
  std::unique_ptr<TestWindow> middle(new TestWindow);
  middle->delegate()->set_can_focus(true);
  middle->window()->set_id(2);
  window_tree_host->window()->AddChild(middle->window());
  focus_client.FocusWindow(middle->window());
  EXPECT_EQ(high->window(), focus_client.GetFocusedWindow());

  // Confirm that requesting focus on the lower z-order windows leaves focus on
  // the highest z-order window.
  focus_client.FocusWindow(low->window());
  EXPECT_EQ(high->window(), focus_client.GetFocusedWindow());
  focus_client.FocusWindow(middle->window());
  EXPECT_EQ(high->window(), focus_client.GetFocusedWindow());

  // Confirm that focus moves to next highest window.
  high.reset();
  EXPECT_EQ(middle->window(), focus_client.GetFocusedWindow());

  // Confirm that focus moves to next highest window.
  middle.reset();
  EXPECT_EQ(low->window(), focus_client.GetFocusedWindow());

  // Confirm that there is no focused window.
  low.reset();
  EXPECT_FALSE(focus_client.GetFocusedWindow());
}

}  // namespace test
}  // namespace chromecast
