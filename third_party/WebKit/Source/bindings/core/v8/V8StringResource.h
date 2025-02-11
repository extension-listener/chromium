/*
 * Copyright (C) 2009 Google Inc. All rights reserved.
 *
 * Redistribution and use in source and binary forms, with or without
 * modification, are permitted provided that the following conditions
 * are met:
 * 1. Redistributions of source code must retain the above copyright
 *    notice, this list of conditions and the following disclaimer.
 * 2. Redistributions in binary form must reproduce the above copyright
 *    notice, this list of conditions and the following disclaimer in the
 *    documentation and/or other materials provided with the distribution.
 *
 * THIS SOFTWARE IS PROVIDED BY APPLE INC. AND ITS CONTRIBUTORS ``AS IS''
 * AND ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO,
 * THE IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR
 * PURPOSE ARE DISCLAIMED. IN NO EVENT SHALL APPLE INC. OR ITS CONTRIBUTORS
 * BE LIABLE FOR ANY DIRECT, INDIRECT, INCIDENTAL, SPECIAL, EXEMPLARY, OR
 * CONSEQUENTIAL DAMAGES (INCLUDING, BUT NOT LIMITED TO, PROCUREMENT OF
 * SUBSTITUTE GOODS OR SERVICES; LOSS OF USE, DATA, OR PROFITS; OR BUSINESS
 * INTERRUPTION) HOWEVER CAUSED AND ON ANY THEORY OF LIABILITY, WHETHER IN
 * CONTRACT, STRICT LIABILITY, OR TORT (INCLUDING NEGLIGENCE OR OTHERWISE)
 * ARISING IN ANY WAY OUT OF THE USE OF THIS SOFTWARE, EVEN IF ADVISED OF
 * THE POSSIBILITY OF SUCH DAMAGE.
 */

#ifndef V8StringResource_h
#define V8StringResource_h

#include "bindings/core/v8/ExceptionState.h"
#include "core/CoreExport.h"
#include "wtf/Allocator.h"
#include "wtf/Threading.h"
#include "wtf/text/AtomicString.h"
#include <v8.h>

namespace blink {

// WebCoreStringResource is a helper class for v8ExternalString. It is used
// to manage the life-cycle of the underlying buffer of the external string.
class WebCoreStringResourceBase {
  USING_FAST_MALLOC(WebCoreStringResourceBase);
  WTF_MAKE_NONCOPYABLE(WebCoreStringResourceBase);

 public:
  explicit WebCoreStringResourceBase(const String& string)
      : m_plainString(string) {
#if DCHECK_IS_ON()
    m_threadId = WTF::currentThread();
#endif
    ASSERT(!string.isNull());
    v8::Isolate::GetCurrent()->AdjustAmountOfExternalAllocatedMemory(
        string.charactersSizeInBytes());
  }

  explicit WebCoreStringResourceBase(const AtomicString& string)
      : m_plainString(string.getString()), m_atomicString(string) {
#if DCHECK_IS_ON()
    m_threadId = WTF::currentThread();
#endif
    ASSERT(!string.isNull());
    v8::Isolate::GetCurrent()->AdjustAmountOfExternalAllocatedMemory(
        string.charactersSizeInBytes());
  }

  virtual ~WebCoreStringResourceBase() {
#if DCHECK_IS_ON()
    ASSERT(m_threadId == WTF::currentThread());
#endif
    int64_t reducedExternalMemory = m_plainString.charactersSizeInBytes();
    if (m_plainString.impl() != m_atomicString.impl() &&
        !m_atomicString.isNull())
      reducedExternalMemory += m_atomicString.charactersSizeInBytes();
    v8::Isolate::GetCurrent()->AdjustAmountOfExternalAllocatedMemory(
        -reducedExternalMemory);
  }

  const String& webcoreString() { return m_plainString; }

  const AtomicString& getAtomicString() {
#if DCHECK_IS_ON()
    ASSERT(m_threadId == WTF::currentThread());
#endif
    if (m_atomicString.isNull()) {
      m_atomicString = AtomicString(m_plainString);
      ASSERT(!m_atomicString.isNull());
      if (m_plainString.impl() != m_atomicString.impl())
        v8::Isolate::GetCurrent()->AdjustAmountOfExternalAllocatedMemory(
            m_atomicString.charactersSizeInBytes());
    }
    return m_atomicString;
  }

 protected:
  // A shallow copy of the string. Keeps the string buffer alive until the V8
  // engine garbage collects it.
  String m_plainString;
  // If this string is atomic or has been made atomic earlier the
  // atomic string is held here. In the case where the string starts
  // off non-atomic and becomes atomic later it is necessary to keep
  // the original string alive because v8 may keep derived pointers
  // into that string.
  AtomicString m_atomicString;

 private:
#if DCHECK_IS_ON()
  WTF::ThreadIdentifier m_threadId;
#endif
};

class WebCoreStringResource16 final
    : public WebCoreStringResourceBase,
      public v8::String::ExternalStringResource {
  WTF_MAKE_NONCOPYABLE(WebCoreStringResource16);

 public:
  explicit WebCoreStringResource16(const String& string)
      : WebCoreStringResourceBase(string) {
    ASSERT(!string.is8Bit());
  }

  explicit WebCoreStringResource16(const AtomicString& string)
      : WebCoreStringResourceBase(string) {
    ASSERT(!string.is8Bit());
  }

  size_t length() const override { return m_plainString.impl()->length(); }
  const uint16_t* data() const override {
    return reinterpret_cast<const uint16_t*>(
        m_plainString.impl()->characters16());
  }
};

class WebCoreStringResource8 final
    : public WebCoreStringResourceBase,
      public v8::String::ExternalOneByteStringResource {
  WTF_MAKE_NONCOPYABLE(WebCoreStringResource8);

 public:
  explicit WebCoreStringResource8(const String& string)
      : WebCoreStringResourceBase(string) {
    ASSERT(string.is8Bit());
  }

  explicit WebCoreStringResource8(const AtomicString& string)
      : WebCoreStringResourceBase(string) {
    ASSERT(string.is8Bit());
  }

  size_t length() const override { return m_plainString.impl()->length(); }
  const char* data() const override {
    return reinterpret_cast<const char*>(m_plainString.impl()->characters8());
  }
};

enum ExternalMode { Externalize, DoNotExternalize };

template <typename StringType>
CORE_EXPORT StringType v8StringToWebCoreString(v8::Local<v8::String>,
                                               ExternalMode);
CORE_EXPORT String int32ToWebCoreString(int value);

// V8StringResource is an adapter class that converts V8 values to Strings
// or AtomicStrings as appropriate, using multiple typecast operators.
enum V8StringResourceMode {
  DefaultMode,
  TreatNullAsEmptyString,
  TreatNullAsNullString,
  TreatNullAndUndefinedAsNullString
};

template <V8StringResourceMode Mode = DefaultMode>
class V8StringResource {
  STACK_ALLOCATED();

 public:
  V8StringResource() : m_mode(Externalize) {}

  V8StringResource(v8::Local<v8::Value> object)
      : m_v8Object(object), m_mode(Externalize) {}

  V8StringResource(const String& string)
      : m_mode(Externalize), m_string(string) {}

  void operator=(v8::Local<v8::Value> object) { m_v8Object = object; }

  void operator=(const String& string) { setString(string); }

  void operator=(std::nullptr_t) { setString(String()); }

  bool prepare() {  // DEPRECATED
    if (prepareFast())
      return true;

    return m_v8Object->ToString(v8::Isolate::GetCurrent()->GetCurrentContext())
        .ToLocal(&m_v8Object);
  }

  bool prepare(v8::Isolate* isolate, ExceptionState& exceptionState) {
    return prepareFast() || prepareSlow(isolate, exceptionState);
  }

  bool prepare(ExceptionState& exceptionState) {  // DEPRECATED
    return prepareFast() ||
           prepareSlow(v8::Isolate::GetCurrent(), exceptionState);
  }

  operator String() const { return toString<String>(); }
  operator AtomicString() const { return toString<AtomicString>(); }

 private:
  bool prepareFast() {
    if (m_v8Object.IsEmpty())
      return true;

    if (!isValid()) {
      setString(fallbackString());
      return true;
    }

    if (LIKELY(m_v8Object->IsString()))
      return true;

    if (LIKELY(m_v8Object->IsInt32())) {
      setString(int32ToWebCoreString(m_v8Object.As<v8::Int32>()->Value()));
      return true;
    }

    m_mode = DoNotExternalize;
    return false;
  }

  bool prepareSlow(v8::Isolate* isolate, ExceptionState& exceptionState) {
    v8::TryCatch tryCatch(isolate);
    if (!m_v8Object->ToString(isolate->GetCurrentContext())
             .ToLocal(&m_v8Object)) {
      exceptionState.rethrowV8Exception(tryCatch.Exception());
      return false;
    }
    return true;
  }

  bool isValid() const;
  String fallbackString() const;

  void setString(const String& string) {
    m_string = string;
    m_v8Object.Clear();  // To signal that String is ready.
  }

  template <class StringType>
  StringType toString() const {
    if (LIKELY(!m_v8Object.IsEmpty()))
      return v8StringToWebCoreString<StringType>(
          const_cast<v8::Local<v8::Value>*>(&m_v8Object)->As<v8::String>(),
          m_mode);

    return StringType(m_string);
  }

  v8::Local<v8::Value> m_v8Object;
  ExternalMode m_mode;
  String m_string;
};

template <>
inline bool V8StringResource<DefaultMode>::isValid() const {
  return true;
}

template <>
inline String V8StringResource<DefaultMode>::fallbackString() const {
  ASSERT_NOT_REACHED();
  return String();
}

template <>
inline bool V8StringResource<TreatNullAsEmptyString>::isValid() const {
  return !m_v8Object->IsNull();
}

template <>
inline String V8StringResource<TreatNullAsEmptyString>::fallbackString() const {
  return emptyString;
}

template <>
inline bool V8StringResource<TreatNullAsNullString>::isValid() const {
  return !m_v8Object->IsNull();
}

template <>
inline String V8StringResource<TreatNullAsNullString>::fallbackString() const {
  return String();
}

template <>
inline bool V8StringResource<TreatNullAndUndefinedAsNullString>::isValid()
    const {
  return !m_v8Object->IsNull() && !m_v8Object->IsUndefined();
}

template <>
inline String
V8StringResource<TreatNullAndUndefinedAsNullString>::fallbackString() const {
  return String();
}

}  // namespace blink

#endif  // V8StringResource_h
