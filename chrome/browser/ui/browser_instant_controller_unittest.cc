// Copyright 2013 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include <stddef.h>

#include "base/gtest_prod_util.h"
#include "base/macros.h"
#include "base/metrics/field_trial.h"
#include "base/run_loop.h"
#include "chrome/browser/chrome_notification_types.h"
#include "chrome/browser/search/instant_service.h"
#include "chrome/browser/search/instant_service_observer.h"
#include "chrome/browser/search/instant_unittest_base.h"
#include "chrome/browser/search/search.h"
#include "chrome/browser/ui/browser_instant_controller.h"
#include "chrome/browser/ui/tabs/tab_strip_model.h"
#include "chrome/common/url_constants.h"
#include "content/public/browser/navigation_handle.h"
#include "content/public/browser/reload_type.h"
#include "content/public/browser/render_process_host.h"
#include "content/public/browser/web_contents.h"
#include "content/public/browser/web_contents_observer.h"
#include "content/public/common/browser_side_navigation_policy.h"

namespace chrome {

namespace {

class BrowserInstantControllerTest : public InstantUnitTestBase {
 public:
  void SetUp() override {
    ASSERT_TRUE(base::FieldTrialList::CreateFieldTrial(
        "EmbeddedSearch", "Group1 use_cacheable_ntp:1"));
    InstantUnitTestBase::SetUp();
  }

 protected:
  friend class FakeWebContentsObserver;
};

struct TabReloadTestCase {
  const char* description;
  const char* start_url;
  bool start_in_instant_process;
  bool should_reload;
  bool end_in_local_ntp;
  bool end_in_instant_process;
};

// Test cases for when Google is the initial, but not final provider.
const TabReloadTestCase kTabReloadTestCasesFinalProviderNotGoogle[] = {
    {"Local Embedded NTP", chrome::kChromeSearchLocalNtpUrl,
     true, true, true, true},
    {"Remote Embedded NTP", "https://www.google.com/newtab",
     true, true, false, false},
    {"Remote Embedded SERP", "https://www.google.com/url?strk&bar=search+terms",
     false, false, false, false},
    {"Other NTP", "https://bar.com/newtab",
     false, false, false, false}
};

// Test cases for when Google is both the initial and final provider.
const TabReloadTestCase kTabReloadTestCasesFinalProviderGoogle[] = {
    {"Local Embedded NTP", chrome::kChromeSearchLocalNtpUrl,
     true, true, true, true},
    {"Remote Embedded NTP", "https://www.google.com/newtab",
     true, false, true, true},
    {"Remote Embedded SERP", "https://www.google.com/url?strk&bar=search+terms",
     false, false, false, false},
    {"Other NTP", "https://bar.com/newtab",
     false, false, false, false}
};


class FakeWebContentsObserver : public content::WebContentsObserver {
 public:
  explicit FakeWebContentsObserver(content::WebContents* contents)
      : WebContentsObserver(contents),
        contents_(contents),
        url_(contents->GetURL()),
        num_reloads_(0) {}

  void DidStartNavigation(
      content::NavigationHandle* navigation_handle) override {
    if (!content::IsBrowserSideNavigationEnabled())
      return;
    if (url_ == navigation_handle->GetURL())
      num_reloads_++;
    current_url_ = navigation_handle->GetURL();
  }

  // TODO: remove this method when PlzNavigate is turned on by default.
  void DidStartNavigationToPendingEntry(
      const GURL& url,
      content::ReloadType reload_type) override {
    if (content::IsBrowserSideNavigationEnabled())
      return;
    if (url_ == url)
      num_reloads_++;
    current_url_ = url;
  }

  const GURL& url() const { return url_; }

  const GURL& current_url() const { return contents_->GetURL(); }

  int num_reloads() const {
    return num_reloads_;
  }

  bool can_go_back() const {
    return contents_->GetController().CanGoBack();
  }

 protected:
  friend class BrowserInstantControllerTest;
  FRIEND_TEST_ALL_PREFIXES(BrowserInstantControllerTest,
                           DefaultSearchProviderChanged);
  FRIEND_TEST_ALL_PREFIXES(BrowserInstantControllerTest,
                           GoogleBaseURLUpdated);

 private:
  content::WebContents* contents_;
  const GURL& url_;
  GURL current_url_;
  int num_reloads_;
};

TEST_F(BrowserInstantControllerTest, DefaultSearchProviderChanged) {
  size_t num_tests = arraysize(kTabReloadTestCasesFinalProviderNotGoogle);
  std::vector<std::unique_ptr<FakeWebContentsObserver>> observers;
  for (size_t i = 0; i < num_tests; ++i) {
    const TabReloadTestCase& test =
        kTabReloadTestCasesFinalProviderNotGoogle[i];
    AddTab(browser(), GURL(test.start_url));
    content::WebContents* contents =
      browser()->tab_strip_model()->GetActiveWebContents();

    // Validate initial instant state.
    EXPECT_EQ(test.start_in_instant_process,
        instant_service_->IsInstantProcess(
          contents->GetRenderProcessHost()->GetID()))
      << test.description;

    // Setup an observer to verify reload or absence thereof.
    observers.push_back(base::MakeUnique<FakeWebContentsObserver>(contents));
  }

  SetUserSelectedDefaultSearchProvider("https://bar.com/");

  for (size_t i = 0; i < num_tests; ++i) {
    FakeWebContentsObserver* observer = observers[i].get();
    const TabReloadTestCase& test =
        kTabReloadTestCasesFinalProviderNotGoogle[i];

    if (test.should_reload) {
      // Validate final instant state.
      EXPECT_EQ(test.end_in_instant_process,
                search::ShouldAssignURLToInstantRenderer(
                    observer->current_url(), profile()))
          << test.description;
    }

    // Ensure only the expected tabs(contents) reloaded.
    base::RunLoop loop;
    loop.RunUntilIdle();
    EXPECT_EQ(test.should_reload ? 1 : 0, observer->num_reloads())
      << test.description;

    if (test.end_in_local_ntp) {
      EXPECT_EQ(GURL(chrome::kChromeSearchLocalNtpUrl), observer->current_url())
          << test.description;
    }
  }
}

TEST_F(BrowserInstantControllerTest, GoogleBaseURLUpdated) {
  const size_t num_tests = arraysize(kTabReloadTestCasesFinalProviderGoogle);
  std::vector<std::unique_ptr<FakeWebContentsObserver>> observers;
  for (size_t i = 0; i < num_tests; ++i) {
    const TabReloadTestCase& test = kTabReloadTestCasesFinalProviderGoogle[i];
    AddTab(browser(), GURL(test.start_url));
    content::WebContents* contents =
      browser()->tab_strip_model()->GetActiveWebContents();

    // Validate initial instant state.
    EXPECT_EQ(test.start_in_instant_process,
        instant_service_->IsInstantProcess(
          contents->GetRenderProcessHost()->GetID()))
      << test.description;

    // Setup an observer to verify reload or absence thereof.
    observers.push_back(base::MakeUnique<FakeWebContentsObserver>(contents));
  }

  NotifyGoogleBaseURLUpdate("https://www.google.es/");

  for (size_t i = 0; i < num_tests; ++i) {
    const TabReloadTestCase& test = kTabReloadTestCasesFinalProviderGoogle[i];
    FakeWebContentsObserver* observer = observers[i].get();

    // Validate final instant state.
    EXPECT_EQ(test.end_in_instant_process,
              search::ShouldAssignURLToInstantRenderer(observer->current_url(),
                                                       profile()))
        << test.description;

    // Ensure only the expected tabs(contents) reloaded.
    base::RunLoop loop;
    loop.RunUntilIdle();
    EXPECT_EQ(test.should_reload ? 1 : 0, observer->num_reloads())
      << test.description;

    if (test.end_in_local_ntp) {
      EXPECT_EQ(GURL(chrome::kChromeSearchLocalNtpUrl), observer->current_url())
          << test.description;
      // The navigation to Local NTP should be definitive i.e. can't go back.
      EXPECT_FALSE(observer->can_go_back());
    }
  }
}

TEST_F(BrowserInstantControllerTest, BrowserWindowLifecycle) {
  std::unique_ptr<BrowserWindow> window(CreateBrowserWindow());
  Browser::CreateParams params(profile());
  params.window = window.get();
  std::unique_ptr<Browser> browser(new Browser(params));
  InstantServiceObserver* bic;
  bic = browser->instant_controller();
  EXPECT_TRUE(IsInstantServiceObserver(bic))
    << "New BrowserInstantController should register as InstantServiceObserver";

  browser.reset(NULL);
  window.reset(NULL);
  EXPECT_FALSE(IsInstantServiceObserver(bic))
    << "New BrowserInstantController should register as InstantServiceObserver";
}

}  // namespace

}  // namespace chrome
