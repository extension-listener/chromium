// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ui/gfx/color_space_win.h"

namespace gfx {

DXVA2_ExtendedFormat ColorSpaceWin::GetExtendedFormat(
    const ColorSpace& color_space) {
  DXVA2_ExtendedFormat format;
  memset(&format, 0, sizeof(format));
  format.SampleFormat = DXVA2_SampleProgressiveFrame;
  format.VideoLighting = DXVA2_VideoLighting_dim;
  format.NominalRange = DXVA2_NominalRange_16_235;
  format.VideoTransferMatrix = DXVA2_VideoTransferMatrix_BT709;
  format.VideoPrimaries = DXVA2_VideoPrimaries_BT709;
  format.VideoTransferFunction = DXVA2_VideoTransFunc_709;

  switch (color_space.range_) {
    case gfx::ColorSpace::RangeID::LIMITED:
      format.NominalRange = DXVA2_NominalRange_16_235;
      break;
    case gfx::ColorSpace::RangeID::FULL:
      format.NominalRange = DXVA2_NominalRange_0_255;
      break;

    case gfx::ColorSpace::RangeID::UNSPECIFIED:
    case gfx::ColorSpace::RangeID::DERIVED:
      // Not handled
      break;
  }

  switch (color_space.matrix_) {
    case gfx::ColorSpace::MatrixID::BT709:
      format.VideoTransferMatrix = DXVA2_VideoTransferMatrix_BT709;
      break;
    case gfx::ColorSpace::MatrixID::BT470BG:
    case gfx::ColorSpace::MatrixID::SMPTE170M:
      format.VideoTransferMatrix = DXVA2_VideoTransferMatrix_BT601;
      break;
    case gfx::ColorSpace::MatrixID::SMPTE240M:
      format.VideoTransferMatrix = DXVA2_VideoTransferMatrix_SMPTE240M;
      break;

    case gfx::ColorSpace::MatrixID::RGB:
    case gfx::ColorSpace::MatrixID::UNSPECIFIED:
    case gfx::ColorSpace::MatrixID::RESERVED:
    case gfx::ColorSpace::MatrixID::FCC:
    case gfx::ColorSpace::MatrixID::YCOCG:
    case gfx::ColorSpace::MatrixID::BT2020_NCL:
    case gfx::ColorSpace::MatrixID::BT2020_CL:
    case gfx::ColorSpace::MatrixID::YDZDX:
    case gfx::ColorSpace::MatrixID::UNKNOWN:
      // Not handled
      break;
  }

  switch (color_space.primaries_) {
    case gfx::ColorSpace::PrimaryID::BT709:
      format.VideoPrimaries = DXVA2_VideoPrimaries_BT709;
      break;
    case gfx::ColorSpace::PrimaryID::BT470M:
      format.VideoPrimaries = DXVA2_VideoPrimaries_BT470_2_SysM;
      break;
    case gfx::ColorSpace::PrimaryID::BT470BG:
      format.VideoPrimaries = DXVA2_VideoPrimaries_BT470_2_SysBG;
      break;
    case gfx::ColorSpace::PrimaryID::SMPTE170M:
      format.VideoPrimaries = DXVA2_VideoPrimaries_SMPTE170M;
      break;
    case gfx::ColorSpace::PrimaryID::SMPTE240M:
      format.VideoPrimaries = DXVA2_VideoPrimaries_SMPTE240M;
      break;

    case gfx::ColorSpace::PrimaryID::RESERVED0:
    case gfx::ColorSpace::PrimaryID::UNSPECIFIED:
    case gfx::ColorSpace::PrimaryID::RESERVED:
    case gfx::ColorSpace::PrimaryID::FILM:
    case gfx::ColorSpace::PrimaryID::BT2020:
    case gfx::ColorSpace::PrimaryID::SMPTEST428_1:
    case gfx::ColorSpace::PrimaryID::SMPTEST431_2:
    case gfx::ColorSpace::PrimaryID::SMPTEST432_1:
    case gfx::ColorSpace::PrimaryID::UNKNOWN:
    case gfx::ColorSpace::PrimaryID::XYZ_D50:
    case gfx::ColorSpace::PrimaryID::ADOBE_RGB:
    case gfx::ColorSpace::PrimaryID::CUSTOM:
      // Not handled
      break;
  }

  switch (color_space.transfer_) {
    case gfx::ColorSpace::TransferID::BT709:
    case gfx::ColorSpace::TransferID::SMPTE170M:
      format.VideoTransferFunction = DXVA2_VideoTransFunc_709;
      break;
    case gfx::ColorSpace::TransferID::SMPTE240M:
      format.VideoTransferFunction = DXVA2_VideoTransFunc_240M;
      break;
    case gfx::ColorSpace::TransferID::GAMMA22:
      format.VideoTransferFunction = DXVA2_VideoTransFunc_22;
      break;
    case gfx::ColorSpace::TransferID::GAMMA28:
      format.VideoTransferFunction = DXVA2_VideoTransFunc_28;
      break;
    case gfx::ColorSpace::TransferID::LINEAR:
    case gfx::ColorSpace::TransferID::LINEAR_HDR:
      format.VideoTransferFunction = DXVA2_VideoTransFunc_10;
      break;
    case gfx::ColorSpace::TransferID::IEC61966_2_1:
      format.VideoTransferFunction = DXVA2_VideoTransFunc_sRGB;
      break;

    case gfx::ColorSpace::TransferID::RESERVED0:
    case gfx::ColorSpace::TransferID::UNSPECIFIED:
    case gfx::ColorSpace::TransferID::RESERVED:
    case gfx::ColorSpace::TransferID::LOG:
    case gfx::ColorSpace::TransferID::LOG_SQRT:
    case gfx::ColorSpace::TransferID::IEC61966_2_4:
    case gfx::ColorSpace::TransferID::BT1361_ECG:
    case gfx::ColorSpace::TransferID::BT2020_10:
    case gfx::ColorSpace::TransferID::BT2020_12:
    case gfx::ColorSpace::TransferID::SMPTEST2084:
    case gfx::ColorSpace::TransferID::SMPTEST428_1:
    case gfx::ColorSpace::TransferID::ARIB_STD_B67:
    case gfx::ColorSpace::TransferID::UNKNOWN:
    case gfx::ColorSpace::TransferID::GAMMA24:
    case gfx::ColorSpace::TransferID::SMPTEST2084_NON_HDR:
    case gfx::ColorSpace::TransferID::CUSTOM:
      // Not handled
      break;
  }

  return format;
}

}  // namespace gfx
