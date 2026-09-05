/* SPDX-FileCopyrightText: 2024 Blender Authors
 *
 * SPDX-License-Identifier: GPL-2.0-or-later */

/** \file
 * \ingroup bli
 *
 * macOS specific implementation for soft-delete (move to Trash).
 * Flipendo: migrado de Objective-C++ a C++ puro. `trashItemAtURL:` no tiene
 * equivalente POSIX/CoreFoundation, asi que se invoca a Cocoa a traves del
 * runtime de Objective-C (objc_msgSend) desde C++ — sin sintaxis ObjC, sin .mm. */

#include <objc/message.h>
#include <objc/runtime.h>

/* Declaradas por el runtime ObjC; no expuestas por los headers C. */
extern "C" void *objc_autoreleasePoolPush(void);
extern "C" void objc_autoreleasePoolPop(void *pool);

#include "BLI_assert.h"
#include "BLI_fileops.h"
#include "BLI_path_utils.hh"

int BLI_delete_soft(const char *filepath, const char **r_error_message)
{
  BLI_assert(!BLI_path_is_rel(filepath));

  void *pool = objc_autoreleasePoolPush();

  Class NSString_c = objc_getClass("NSString");
  Class NSFileManager_c = objc_getClass("NSFileManager");
  Class NSURL_c = objc_getClass("NSURL");

  using MsgStr = id (*)(Class, SEL, const char *);
  using MsgCls = id (*)(Class, SEL);
  using MsgId = id (*)(Class, SEL, id);
  using MsgTrash = signed char (*)(id, SEL, id, id, id);

  id path_string = reinterpret_cast<MsgStr>(objc_msgSend)(
      NSString_c, sel_registerName("stringWithUTF8String:"), filepath);
  id file_manager = reinterpret_cast<MsgCls>(objc_msgSend)(
      NSFileManager_c, sel_registerName("defaultManager"));
  id target_url = reinterpret_cast<MsgId>(objc_msgSend)(
      NSURL_c, sel_registerName("fileURLWithPath:"), path_string);

  const signed char ok = reinterpret_cast<MsgTrash>(objc_msgSend)(
      file_manager,
      sel_registerName("trashItemAtURL:resultingItemURL:error:"),
      target_url,
      nullptr,
      nullptr);

  objc_autoreleasePoolPop(pool);

  if (!ok) {
    *r_error_message = "The macOS API call to move file or directory to Trash failed";
    return -1;
  }
  return 0;
}
