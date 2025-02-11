// Copyright 2016 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_CHROMEOS_LOGIN_SCREENS_ARC_TERMS_OF_SERVICE_SCREEN_ACTOR_H_
#define CHROME_BROWSER_CHROMEOS_LOGIN_SCREENS_ARC_TERMS_OF_SERVICE_SCREEN_ACTOR_H_

#include <string>

#include "base/macros.h"

namespace chromeos {

class ArcTermsOfServiceScreenActorObserver;

// Interface for dependency injection between TermsOfServiceScreen and its
// WebUI representation.
class ArcTermsOfServiceScreenActor {
 public:
  virtual ~ArcTermsOfServiceScreenActor() = default;

  // Adds/Removes observer for actor.
  virtual void AddObserver(ArcTermsOfServiceScreenActorObserver* observer) = 0;
  virtual void RemoveObserver(
      ArcTermsOfServiceScreenActorObserver* observer) = 0;

  // Shows the contents of the screen.
  virtual void Show() = 0;

  // Hides the contents of the screen.
  virtual void Hide() = 0;

 protected:
  ArcTermsOfServiceScreenActor() = default;

 private:
  DISALLOW_COPY_AND_ASSIGN(ArcTermsOfServiceScreenActor);
};

}  // namespace chromeos

#endif  // CHROME_BROWSER_CHROMEOS_LOGIN_SCREENS_ARC_TERMS_OF_SERVICE_SCREEN_ACTOR_H_
