/* SPDX-FileCopyrightText: 2020 Blender Authors
 *
 * SPDX-License-Identifier: GPL-2.0-or-later */

/** \file
 * \ingroup bli
 *
 * macOS specific implementations for storage.
 * Flipendo: migrado de Objective-C++ a C++ puro usando la C-API de
 * CoreFoundation (URLs, bookmarks/alias, resource keys) + POSIX. Sin Cocoa/ObjC. */

#include <CoreFoundation/CoreFoundation.h>

#include <string>
#include <sys/xattr.h>
#include <unistd.h>

#include "BLI_fileops.h"
#include "BLI_path_utils.hh"
#include "BLI_string.h"

/* Extended file attribute used by OneDrive to mark placeholder files. */
static const char *ONEDRIVE_RECALLONOPEN_ATTRIBUTE = "com.microsoft.OneDrive.RecallOnOpen";

/* Crea un CFURL de fichero desde una ruta UTF-8. Devuelve nullptr si falla. */
static CFURLRef cfurl_from_path(const char *filepath)
{
  CFStringRef path = CFStringCreateWithCString(nullptr, filepath, kCFStringEncodingUTF8);
  if (path == nullptr) {
    return nullptr;
  }
  CFURLRef url = CFURLCreateWithFileSystemPath(nullptr, path, kCFURLPOSIXPathStyle, false);
  CFRelease(path);
  return url;
}

/* Lee una propiedad booleana de recurso (kCFURLIs...Key). */
static bool cfurl_bool_property(CFURLRef url, CFStringRef key, bool fallback)
{
  CFBooleanRef value = nullptr;
  bool result = fallback;
  if (CFURLCopyResourcePropertyForKey(url, key, &value, nullptr) && value != nullptr) {
    result = CFBooleanGetValue(value);
    CFRelease(value);
  }
  return result;
}

bool BLI_file_alias_target(const char *filepath, char r_targetpath[FILE_MAXDIR])
{
  CFURLRef shortcut = cfurl_from_path(filepath);
  if (shortcut == nullptr) {
    return false;
  }

  /* Extrae los datos de bookmark almacenados en el fichero alias. */
  CFErrorRef error = nullptr;
  CFDataRef bookmark = CFURLCreateBookmarkDataFromFile(nullptr, shortcut, &error);
  if (bookmark == nullptr) {
    CFRelease(shortcut);
    return false;
  }

  Boolean is_stale = false;
  CFURLRef target = CFURLCreateByResolvingBookmarkData(
      nullptr,
      bookmark,
      kCFBookmarkResolutionWithoutUIMask | kCFBookmarkResolutionWithoutMountingMask,
      nullptr,
      nullptr,
      &is_stale,
      &error);
  CFRelease(bookmark);

  bool ok = false;
  if (target != nullptr) {
    const bool same = CFEqual(shortcut, target);
    if (CFURLGetFileSystemRepresentation(
            target, true, reinterpret_cast<UInt8 *>(r_targetpath), FILE_MAXDIR)) {
      ok = !same;
    }
    CFRelease(target);
  }
  CFRelease(shortcut);
  return ok;
}

static bool find_attribute(const std::string &attributes, const char *search_attribute)
{
  const char *end = attributes.data() + attributes.size();
  for (const char *item = attributes.data(); item < end; item += strlen(item) + 1) {
    if (STREQ(item, search_attribute)) {
      return true;
    }
  }
  return false;
}

static bool test_onedrive_file_is_placeholder(const char *path)
{
  ssize_t size = listxattr(path, nullptr, 0, XATTR_NOFOLLOW);
  if (size < 1) {
    return false;
  }
  std::string attributes(size, '\0');
  size = listxattr(path, attributes.data(), size, XATTR_NOFOLLOW);
  if (size < 1) {
    return false;
  }
  return find_attribute(attributes, ONEDRIVE_RECALLONOPEN_ATTRIBUTE);
}

static bool test_file_is_offline(const char *path)
{
  return test_onedrive_file_is_placeholder(path);
}

eFileAttributes BLI_file_attributes(const char *path)
{
  int ret = 0;
  CFURLRef url = cfurl_from_path(path);
  if (url == nullptr) {
    return (eFileAttributes)0;
  }

  const bool is_offline = test_file_is_offline(path);
  const bool is_alias = cfurl_bool_property(url, kCFURLIsAliasFileKey, false);
  const bool is_hidden = cfurl_bool_property(url, kCFURLIsHiddenKey, false);
  /* Querying readable/writable on OneDrive placeholders triggers their download,
   * so for offline files we assume readable and not writable, as the original. */
  const bool is_readable = is_offline || (access(path, R_OK) == 0);
  const bool is_writable = is_offline || (access(path, W_OK) == 0);
  CFRelease(url);

  if (is_alias) {
    ret |= FILE_ATTR_ALIAS;
  }
  if (is_hidden) {
    ret |= FILE_ATTR_HIDDEN;
  }
  if (is_readable && !is_writable) {
    ret |= FILE_ATTR_READONLY;
  }
  if (!is_readable) {
    ret |= FILE_ATTR_SYSTEM;
  }
  if (is_offline) {
    ret |= FILE_ATTR_OFFLINE;
  }
  return (eFileAttributes)ret;
}

char *BLI_current_working_dir(char *dir, const size_t maxncpy)
{
  char path[PATH_MAX];
  if (getcwd(path, sizeof(path)) == nullptr) {
    return nullptr;
  }
  BLI_strncpy(dir, path, maxncpy);
  return dir;
}

bool BLI_change_working_dir(const char *dir)
{
  return chdir(dir) == 0;
}
