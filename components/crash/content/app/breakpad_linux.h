// Copyright 2013 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

// Public interface for enabling Breakpad on Linux systems.

#ifndef COMPONENTS_CRASH_CONTENT_APP_BREAKPAD_LINUX_H_
#define COMPONENTS_CRASH_CONTENT_APP_BREAKPAD_LINUX_H_

#include <string>

#include "build/build_config.h"

namespace breakpad {

// Turns on the crash reporter in any process.
extern void InitCrashReporter(const std::string& process_type);

#if defined(OS_ANDROID)

const char kWebViewSingleProcessType[] = "webview";
const char kBrowserProcessType[] = "browser";

// Enables the crash reporter in child processes.
extern void InitNonBrowserCrashReporterForAndroid(
    const std::string& process_type);

// Enables *micro*dump only. Can be called from any process.
extern void InitMicrodumpCrashHandlerIfNecessary(
    const std::string& process_type);

extern void AddGpuFingerprintToMicrodumpCrashHandler(
    const std::string& gpu_fingerprint);

// Calling SuppressDumpGeneration causes subsequent crashes to not
// generate dumps. Calling base::debug::DumpWithoutCrashing will still
// generate a dump.
extern void SuppressDumpGeneration();

// Calling SetShouldSanitizeDumps determines whether or not subsequent
// crash dumps should be sanitized. Sanitized dumps still contain
// enough stack information to unwind crashes, but other stack data is
// erased.
extern void SetShouldSanitizeDumps(bool sanitize_dumps);

// Inform breakpad of an address within the text section that is
// considered interesting for the purpose of crashes so that this can
// be used to elide microdumps that do not reference interesting
// code. Minidumps will still be generated, but stacks from threads
// that do not reference the principal mapping will not be included.
// The full interesting address range is determined by looking up the
// memory mapping that contains |addr|.
extern void SetSkipDumpIfPrincipalMappingNotReferenced(
    uintptr_t address_within_principal_mapping);
#endif

// Checks if crash reporting is enabled. Note that this is not the same as
// being opted into metrics reporting (and crash reporting), which controls
// whether InitCrashReporter() is called.
bool IsCrashReporterEnabled();

// Generates a minidump on demand for this process, writing it to |dump_fd|.
void GenerateMinidumpOnDemandForAndroid(int dump_fd);
}  // namespace breakpad

#endif  // COMPONENTS_CRASH_CONTENT_APP_BREAKPAD_LINUX_H_
