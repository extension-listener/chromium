// Copyright 2015 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef NavigatorPlugins_h
#define NavigatorPlugins_h

#include "core/frame/Navigator.h"
#include "platform/Supplementable.h"

namespace blink {

class DOMMimeTypeArray;
class DOMPluginArray;
class LocalFrame;
class Navigator;

class NavigatorPlugins final : public GarbageCollected<NavigatorPlugins>,
                               public Supplement<Navigator> {
  USING_GARBAGE_COLLECTED_MIXIN(NavigatorPlugins);

 public:
  static NavigatorPlugins& from(Navigator&);
  static NavigatorPlugins* toNavigatorPlugins(Navigator&);

  static DOMPluginArray* plugins(Navigator&);
  static DOMMimeTypeArray* mimeTypes(Navigator&);
  static bool javaEnabled(Navigator&);

  DECLARE_VIRTUAL_TRACE();

 private:
  explicit NavigatorPlugins(Navigator&);

  static const char* supplementName();

  DOMPluginArray* plugins(LocalFrame*) const;
  DOMMimeTypeArray* mimeTypes(LocalFrame*) const;

  mutable Member<DOMPluginArray> m_plugins;
  mutable Member<DOMMimeTypeArray> m_mimeTypes;
};

}  // namespace blink

#endif  // NavigatorPlugins_h
