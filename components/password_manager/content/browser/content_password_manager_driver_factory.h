// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef COMPONENTS_PASSWORD_MANAGER_CONTENT_BROWSER_CONTENT_PASSWORD_MANAGER_DRIVER_FACTORY_H_
#define COMPONENTS_PASSWORD_MANAGER_CONTENT_BROWSER_CONTENT_PASSWORD_MANAGER_DRIVER_FACTORY_H_

#include <map>
#include <memory>

#include "base/compiler_specific.h"
#include "base/macros.h"
#include "base/supports_user_data.h"
#include "components/autofill/content/common/autofill_driver.mojom.h"
#include "components/password_manager/core/browser/password_autofill_manager.h"
#include "components/password_manager/core/browser/password_generation_manager.h"
#include "components/password_manager/core/browser/password_manager.h"
#include "components/password_manager/core/browser/password_manager_driver.h"
#include "content/public/browser/web_contents_observer.h"
#include "third_party/WebKit/public/platform/modules/sensitive_input_visibility/sensitive_input_visibility_service.mojom.h"

namespace content {
class WebContents;
}

namespace password_manager {

class ContentPasswordManagerDriver;

// Creates and owns ContentPasswordManagerDrivers. There is one
// factory per WebContents, and one driver per render frame.
class ContentPasswordManagerDriverFactory
    : public content::WebContentsObserver,
      public base::SupportsUserData::Data {
 public:
  static void CreateForWebContents(content::WebContents* web_contents,
                                   PasswordManagerClient* client,
                                   autofill::AutofillClient* autofill_client);
  ~ContentPasswordManagerDriverFactory() override;

  static ContentPasswordManagerDriverFactory* FromWebContents(
      content::WebContents* web_contents);

  static void BindPasswordManagerDriver(
      content::RenderFrameHost* render_frame_host,
      autofill::mojom::PasswordManagerDriverRequest request);

  static void BindSensitiveInputVisibilityService(
      content::RenderFrameHost* render_frame_host,
      blink::mojom::SensitiveInputVisibilityServiceRequest request);

  ContentPasswordManagerDriver* GetDriverForFrame(
      content::RenderFrameHost* render_frame_host);

  // Requests all drivers to inform their renderers whether
  // chrome://password-manager-internals is available.
  void RequestSendLoggingAvailability();

  // content::WebContentsObserver:
  void RenderFrameCreated(content::RenderFrameHost* render_frame_host) override;
  void RenderFrameDeleted(content::RenderFrameHost* render_frame_host) override;
  void DidFinishNavigation(
      content::NavigationHandle* navigation_handle) override;

 private:
  ContentPasswordManagerDriverFactory(
      content::WebContents* web_contents,
      PasswordManagerClient* client,
      autofill::AutofillClient* autofill_client);

  std::map<content::RenderFrameHost*,
           std::unique_ptr<ContentPasswordManagerDriver>>
      frame_driver_map_;

  PasswordManagerClient* password_client_;
  autofill::AutofillClient* autofill_client_;

  DISALLOW_COPY_AND_ASSIGN(ContentPasswordManagerDriverFactory);
};

}  // namespace password_manager

#endif  // COMPONENTS_PASSWORD_MANAGER_CONTENT_BROWSER_CONTENT_PASSWORD_MANAGER_DRIVER_FACTORY_H_
