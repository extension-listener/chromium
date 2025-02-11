// Copyright 2016 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef COMPONENTS_SUBRESOURCE_FILTER_CONTENT_RENDERER_SUBRESOURCE_FILTER_AGENT_H_
#define COMPONENTS_SUBRESOURCE_FILTER_CONTENT_RENDERER_SUBRESOURCE_FILTER_AGENT_H_

#include <memory>
#include <vector>

#include "base/macros.h"
#include "base/memory/weak_ptr.h"
#include "components/subresource_filter/content/common/document_load_statistics.h"
#include "components/subresource_filter/core/common/activation_level.h"
#include "content/public/renderer/render_frame_observer.h"
#include "url/gurl.h"

class GURL;

namespace blink {
class WebDocumentSubresourceFilter;
}  // namespace blink

namespace subresource_filter {

class UnverifiedRulesetDealer;
class DocumentSubresourceFilter;

// The renderer-side agent of the ContentSubresourceFilterDriver. There is one
// instance per RenderFrame, responsible for setting up the subresource filter
// for the ongoing provisional document load in the frame when instructed to do
// so by the driver.
class SubresourceFilterAgent
    : public content::RenderFrameObserver,
      public base::SupportsWeakPtr<SubresourceFilterAgent> {
 public:
  // The |ruleset_dealer| must not be null and must outlive this instance. The
  // |render_frame| may be null in unittests.
  explicit SubresourceFilterAgent(content::RenderFrame* render_frame,
                                  UnverifiedRulesetDealer* ruleset_dealer);
  ~SubresourceFilterAgent() override;

 protected:
  // Below methods are protected virtual so they can be mocked out in tests.

  // Returns the URLs of documents loaded into nested frames starting with the
  // current frame and ending with the main frame. The returned array is
  // guaranteed to have at least one element.
  virtual std::vector<GURL> GetAncestorDocumentURLs();

  // Injects the provided subresource |filter| into the DocumentLoader
  // orchestrating the most recently committed load.
  virtual void SetSubresourceFilterForCommittedLoad(
      std::unique_ptr<blink::WebDocumentSubresourceFilter> filter);

  // Informs the browser that the first subresource load has been disallowed for
  // the most recently committed load. Not called if all resources are allowed.
  virtual void SignalFirstSubresourceDisallowedForCommittedLoad();

  // Sends statistics about the DocumentSubresourceFilter's work to the browser.
  virtual void SendDocumentLoadStatistics(
      const DocumentLoadStatistics& statistics);

 private:
  void OnActivateForProvisionalLoad(ActivationLevel activation_level,
                                    const GURL& url,
                                    bool measure_performance);
  void RecordHistogramsOnLoadCommitted();
  void RecordHistogramsOnLoadFinished();

  // content::RenderFrameObserver:
  void OnDestruct() override;
  void DidStartProvisionalLoad(blink::WebDataSource* data_source) override;
  void DidCommitProvisionalLoad(bool is_new_navigation,
                                bool is_same_page_navigation) override;
  void DidFinishLoad() override;
  bool OnMessageReceived(const IPC::Message& message) override;

  // Owned by the ChromeContentRendererClient and outlives us.
  UnverifiedRulesetDealer* ruleset_dealer_;

  ActivationLevel activation_level_for_provisional_load_;
  GURL url_for_provisional_load_;
  bool measure_performance_ = false;
  base::WeakPtr<DocumentSubresourceFilter> filter_for_last_committed_load_;

  DISALLOW_COPY_AND_ASSIGN(SubresourceFilterAgent);
};

}  // namespace subresource_filter

#endif  // COMPONENTS_SUBRESOURCE_FILTER_CONTENT_RENDERER_SUBRESOURCE_FILTER_AGENT_H_
