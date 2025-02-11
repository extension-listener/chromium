// Copyright (c) 2011 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/renderer/safe_browsing/threat_dom_details.h"

#include <memory>

#include "base/strings/stringprintf.h"
#include "chrome/test/base/chrome_render_view_test.h"
#include "components/safe_browsing/common/safebrowsing_messages.h"
#include "content/public/renderer/render_view.h"
#include "net/base/escape.h"
#include "third_party/WebKit/public/web/WebRuntimeFeatures.h"
#include "ui/native_theme/native_theme_switches.h"

typedef ChromeRenderViewTest ThreatDOMDetailsTest;

TEST_F(ThreatDOMDetailsTest, Everything) {
  blink::WebRuntimeFeatures::enableOverlayScrollbars(
      ui::IsOverlayScrollbarEnabled());
  std::unique_ptr<safe_browsing::ThreatDOMDetails> details(
      safe_browsing::ThreatDOMDetails::Create(view_->GetMainRenderFrame()));
  // Lower kMaxNodes for the test. Loading 500 subframes in a
  // debug build takes a while.
  details->kMaxNodes = 50;

  const char urlprefix[] = "data:text/html;charset=utf-8,";

  {
    // A page with an internal script
    std::string html = "<html><head><script></script></head></html>";
    LoadHTML(html.c_str());
    std::vector<SafeBrowsingHostMsg_ThreatDOMDetails_Node> params;
    details->ExtractResources(&params);
    ASSERT_EQ(1u, params.size());
    auto& param = params[0];
    EXPECT_EQ(GURL(urlprefix + html), param.url);
    EXPECT_EQ(0, param.node_id);
    EXPECT_EQ(0, param.parent_node_id);
    EXPECT_TRUE(param.child_node_ids.empty());
  }

  {
    // A page with 2 external scripts.
    // Note: This part of the test causes 2 leaks: LEAK: 5 WebCoreNode
    // LEAK: 2 CachedResource.
    GURL script1_url("data:text/javascript;charset=utf-8,var a=1;");
    GURL script2_url("data:text/javascript;charset=utf-8,var b=2;");
    std::string html = "<html><head><script src=\"" + script1_url.spec() +
                       "\"></script><script src=\"" + script2_url.spec() +
                       "\"></script></head></html>";
    GURL url(urlprefix + html);

    LoadHTML(html.c_str());
    std::vector<SafeBrowsingHostMsg_ThreatDOMDetails_Node> params;
    details->ExtractResources(&params);
    ASSERT_EQ(3u, params.size());
    auto& param = params[0];
    EXPECT_EQ(script1_url, param.url);
    EXPECT_EQ("SCRIPT", param.tag_name);
    EXPECT_EQ(1, param.node_id);
    EXPECT_EQ(0, param.parent_node_id);
    EXPECT_TRUE(param.child_node_ids.empty());

    param = params[1];
    EXPECT_EQ(script2_url, param.url);
    EXPECT_EQ("SCRIPT", param.tag_name);
    EXPECT_EQ(2, param.node_id);
    EXPECT_EQ(0, param.parent_node_id);
    EXPECT_TRUE(param.child_node_ids.empty());

    param = params[2];
    EXPECT_EQ(url, param.url);
    EXPECT_EQ(0, param.node_id);
    EXPECT_EQ(0, param.parent_node_id);
    EXPECT_TRUE(param.child_node_ids.empty());
  }

  {
    // A page with an iframe which in turn contains an iframe.
    //  html
    //   \ iframe1
    //    \ iframe2
    // Since ThreatDOMDetails is a RenderFrameObserver, it will only
    // extract resources from the frame it assigned to (in this case,
    // the main frame). Extracting resources from all frames within a
    // page is covered in SafeBrowsingBlockingPageBrowserTest.
    // In this example, ExtractResources() will still touch iframe1
    // since it is the direct child of the main frame, but it would not
    // go inside of iframe1.
    std::string iframe2_html = "<html><body>iframe2</body></html>";
    GURL iframe2_url(urlprefix + iframe2_html);
    std::string iframe1_html = "<iframe src=\"" +
                               net::EscapeForHTML(iframe2_url.spec()) +
                               "\"></iframe>";
    GURL iframe1_url(urlprefix + iframe1_html);
    std::string html = "<html><head><iframe src=\"" +
                       net::EscapeForHTML(iframe1_url.spec()) +
                       "\"></iframe></head></html>";
    GURL url(urlprefix + html);

    LoadHTML(html.c_str());
    std::vector<SafeBrowsingHostMsg_ThreatDOMDetails_Node> params;
    details->ExtractResources(&params);
    ASSERT_EQ(2u, params.size());

    auto& param = params[0];
    EXPECT_EQ(iframe1_url, param.url);
    EXPECT_EQ(url, param.parent);
    EXPECT_EQ("IFRAME", param.tag_name);
    EXPECT_EQ(0u, param.children.size());
    EXPECT_EQ(1, param.node_id);
    EXPECT_EQ(0, param.parent_node_id);
    EXPECT_TRUE(param.child_node_ids.empty());

    param = params[1];
    EXPECT_EQ(url, param.url);
    EXPECT_EQ(GURL(), param.parent);
    EXPECT_EQ(1u, param.children.size());
    EXPECT_EQ(0, param.node_id);
    EXPECT_EQ(0, param.parent_node_id);
    EXPECT_TRUE(param.child_node_ids.empty());
  }

  {
    // Test >50 subframes.
    std::string html;
    for (int i = 0; i < 55; ++i) {
      // The iframe contents is just a number.
      GURL iframe_url(base::StringPrintf("%s%d", urlprefix, i));
      html += "<iframe src=\"" + net::EscapeForHTML(iframe_url.spec()) +
              "\"></iframe>";
    }
    GURL url(urlprefix + html);

    LoadHTML(html.c_str());
    std::vector<SafeBrowsingHostMsg_ThreatDOMDetails_Node> params;
    details->ExtractResources(&params);
    ASSERT_EQ(51u, params.size());

    // The element nodes should all have node IDs.
    for (size_t i = 0; i < params.size() - 1; ++i) {
      auto& param = params[i];
      const int expected_id = i + 1;
      EXPECT_EQ(expected_id, param.node_id);
      EXPECT_EQ(0, param.parent_node_id);
      EXPECT_TRUE(param.child_node_ids.empty());
    }
  }

  {
    // A page with >50 scripts, to verify kMaxNodes.
    std::string html;
    for (int i = 0; i < 55; ++i) {
      // The iframe contents is just a number.
      GURL script_url(base::StringPrintf("%s%d", urlprefix, i));
      html += "<script src=\"" + net::EscapeForHTML(script_url.spec()) +
              "\"></script>";
    }
    GURL url(urlprefix + html);

    LoadHTML(html.c_str());
    std::vector<SafeBrowsingHostMsg_ThreatDOMDetails_Node> params;
    details->ExtractResources(&params);
    ASSERT_EQ(51u, params.size());

    // The element nodes should all have node IDs.
    for (size_t i = 0; i < params.size() - 1; ++i) {
      auto& param = params[i];
      const int expected_id = i + 1;
      EXPECT_EQ(expected_id, param.node_id);
      EXPECT_EQ(0, param.parent_node_id);
      EXPECT_TRUE(param.child_node_ids.empty());
    }
  }
}
