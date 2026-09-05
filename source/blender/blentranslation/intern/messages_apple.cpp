/* SPDX-FileCopyrightText: 2012 Blender Authors
 *
 * SPDX-License-Identifier: GPL-2.0-or-later */

/** \file
 * \ingroup blt
 *
 * Flipendo: migrado de Objective-C++ (messages_apple.mm) a C++ puro usando la
 * C-API de CoreFoundation, eliminando la dependencia de Cocoa/ObjC. */

#include "messages.hh"

#include <CoreFoundation/CoreFoundation.h>

#include <string>

namespace blender::locale {

#if !defined(WITH_HEADLESS) && !defined(WITH_GHOST_SDL)

static std::string cfstring_to_std(CFStringRef s)
{
  if (s == nullptr) {
    return "";
  }
  char buf[256];
  if (CFStringGetCString(s, buf, sizeof(buf), kCFStringEncodingUTF8)) {
    return std::string(buf);
  }
  return "";
}

/* Get current locale (language + country), C++ puro via CoreFoundation. */
std::string macos_user_locale()
{
  CFLocaleRef locale = CFLocaleCopyCurrent();
  std::string lang = cfstring_to_std(
      static_cast<CFStringRef>(CFLocaleGetValue(locale, kCFLocaleLanguageCode)));
  std::string country = cfstring_to_std(
      static_cast<CFStringRef>(CFLocaleGetValue(locale, kCFLocaleCountryCode)));
  CFRelease(locale);

  std::string result;
  if (!lang.empty() && !country.empty()) {
    result = lang + "_" + country;
  }
  else {
    result = lang;
  }
  return result + ".UTF-8";
}
#endif

}  // namespace blender::locale
