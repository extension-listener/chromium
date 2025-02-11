// Copyright 2015 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/chromeos/login/screens/user_image_model.h"

#include "chrome/browser/chromeos/login/wizard_controller.h"

namespace chromeos {

const char UserImageModel::kContextKeyIsCameraPresent[] = "isCameraPresent";
const char UserImageModel::kContextKeyProfilePictureDataURL[] =
    "profilePictureDataURL";
const char UserImageModel::kContextKeySelectedImageURL[] = "selectedImageURL";
const char UserImageModel::kContextKeyHasGaiaAccount[] = "hasGaiaAccount";

UserImageModel::UserImageModel(BaseScreenDelegate* base_screen_delegate)
    : BaseScreen(base_screen_delegate, OobeScreen::SCREEN_USER_IMAGE_PICKER) {}

UserImageModel::~UserImageModel() {}

}  // namespace chromeos
