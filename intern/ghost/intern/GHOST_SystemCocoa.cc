/* SPDX-FileCopyrightText: 2001-2002 NaN Holding BV. All rights reserved.
 *
 * SPDX-License-Identifier: GPL-2.0-or-later */

/** \file
 * \ingroup GHOST
 *
 * C++ PURO. Era `GHOST_SystemCocoa.mm`, el ULTIMO Objective-C++ de `intern/ghost`.
 *
 * Aqui vive el delegado de la APLICACION (el que decide si Blender puede cerrarse, el
 * que recibe los ficheros que se abren desde el Finder) y el bucle de eventos que
 * reparte cada tecla y cada movimiento del raton.
 *
 * La traduccion es DELIBERADAMENTE CONSERVADORA: las ~300 lineas de `convertKey` y
 * `convertButton` y toda la logica de despacho de eventos son C++ que ya estaba y se
 * han dejado BYTE A BYTE como estaban. Solo se ha traducido lo que era sintaxis de
 * Objective-C. Asi la revision se reduce a mirar lo que de verdad cambio.
 *
 * Lo que este fichero necesito y los anteriores no:
 *   - un BLOQUE de Clang fabricado a mano desde C++ estandar (ver `ghost_objc::Block`),
 *     porque `showSamplerWithSelectionHandler:` no acepta otra cosa;
 *   - `dispatch_after_f` en vez de `dispatch_after`, que es la variante con puntero a
 *     funcion y evita un segundo bloque;
 *   - los bucles de enumeracion rapida (`for (x in coleccion)`) pasados a indice, que
 *     para un NSArray es exactamente lo mismo;
 *   - los literales `@[ ... ]`, `@{ ... }` y `@YES` escritos con sus constructores.
 */

#include "GHOST_SystemCocoa.hh"

#include "GHOST_EventButton.hh"
#include "GHOST_EventCursor.hh"
#include "GHOST_EventDragnDrop.hh"
#include "GHOST_EventKey.hh"
#include "GHOST_EventString.hh"
#include "GHOST_EventTrackpad.hh"
#include "GHOST_EventWheel.hh"
#include "GHOST_ObjCRuntime.hh"
#include "GHOST_TimerManager.hh"
#include "GHOST_TimerTask.hh"
#include "GHOST_WindowCocoa.hh"
#include "GHOST_WindowManager.hh"

#ifdef WITH_METAL_BACKEND
#  include "GHOST_ContextCGL.hh"
#endif

#ifdef WITH_VULKAN_BACKEND
#  include "GHOST_ContextVK.hh"
#endif

#ifdef WITH_INPUT_NDOF
#  include "GHOST_NDOFManagerCocoa.hh"
#endif

#include "AssertMacros.h"

/* Carbon y CoreGraphics son C puro: se incluyen desde un `.cc` sin arrastrar AppKit.
 * De Carbon salen los `kVK_*` del mapa de teclas y `GetCurrentEventButtonState`. */
#include <Carbon/Carbon.h>
#include <CoreGraphics/CoreGraphics.h>
#include <dispatch/dispatch.h>

#include <sys/sysctl.h>
#include <sys/time.h>
#include <sys/types.h>

#include <mach/mach_time.h>

using ghost_objc::msg;

/* -------------------------------------------------------------------------
 * Simbolos globales de AppKit y Foundation.
 *
 * Se usan los SIMBOLOS de verdad y no cadenas escritas a mano: si Apple renombra uno,
 * lo dice el enlazador en vez de fallar en silencio.
 */
extern "C" {
extern id NSApp;
extern id NSDefaultRunLoopMode;
extern id NSWindowWillCloseNotification;
extern id NSPasteboardTypeString;
extern id NSPasteboardTypeTIFF;
extern id NSPasteboardTypePNG;
extern id NSPasteboardTypeFileURL;
extern id NSPasteboardURLReadingFileURLsOnlyKey;
extern id NSPasteboardURLReadingContentsConformToTypesKey;
extern id NSDeviceRGBColorSpace;
}

namespace {

using NSUInteger_ = unsigned long;
using NSInteger_ = long;

/* Tipos de Foundation que el codigo heredado nombra directamente. En 64 bits
 * `NSPoint`, `NSSize` y `NSRect` SON los tipos de CoreGraphics (el SDK los define asi),
 * y `unichar` es `unsigned short`. Se declaran aqui para no tener que tocar las ~300
 * lineas de `convertKey` ni la logica de eventos, que se dejan byte a byte. */
using unichar = unsigned short;
using NSTimeInterval = double;
using NSPoint = CGPoint;
using NSSize = CGSize;
using NSRect = CGRect;

/* Constantes de AppKit. Las 40 estan comprobadas con `static_assert` contra el SDK 26.5
 * en la sonda de constantes (ver informe GHOST-1). Un valor equivocado aqui no da
 * ningun aviso: convierte un clic derecho en uno izquierdo, o una rueda en un giro. */
constexpr NSUInteger_ kNSWindowStyleMaskTitled = 1 << 0;
constexpr NSUInteger_ kNSWindowStyleMaskClosable = 1 << 1;
constexpr NSUInteger_ kNSWindowStyleMaskMiniaturizable = 1 << 2;

constexpr NSUInteger_ kNSEventModifierFlagShift = 1 << 17;
constexpr NSUInteger_ kNSEventModifierFlagControl = 1 << 18;
constexpr NSUInteger_ kNSEventModifierFlagOption = 1 << 19;
constexpr NSUInteger_ kNSEventModifierFlagCommand = 1 << 20;

constexpr NSUInteger_ kNSEventMaskAny = ~NSUInteger_(0);

/* NSEventType */
constexpr NSUInteger_ kNSEventTypeLeftMouseDown = 1;
constexpr NSUInteger_ kNSEventTypeLeftMouseUp = 2;
constexpr NSUInteger_ kNSEventTypeRightMouseDown = 3;
constexpr NSUInteger_ kNSEventTypeRightMouseUp = 4;
constexpr NSUInteger_ kNSEventTypeMouseMoved = 5;
constexpr NSUInteger_ kNSEventTypeLeftMouseDragged = 6;
constexpr NSUInteger_ kNSEventTypeRightMouseDragged = 7;
constexpr NSUInteger_ kNSEventTypeKeyDown = 10;
constexpr NSUInteger_ kNSEventTypeKeyUp = 11;
constexpr NSUInteger_ kNSEventTypeFlagsChanged = 12;
constexpr NSUInteger_ kNSEventTypeRotate = 18;
constexpr NSUInteger_ kNSEventTypeScrollWheel = 22;
constexpr NSUInteger_ kNSEventTypeTabletPoint = 23;
constexpr NSUInteger_ kNSEventTypeTabletProximity = 24;
constexpr NSUInteger_ kNSEventTypeOtherMouseDown = 25;
constexpr NSUInteger_ kNSEventTypeOtherMouseUp = 26;
constexpr NSUInteger_ kNSEventTypeOtherMouseDragged = 27;
constexpr NSUInteger_ kNSEventTypeMagnify = 30;
constexpr NSUInteger_ kNSEventTypeSmartMagnify = 32;

/* NSEventSubtype */
constexpr short kNSEventSubtypeTabletPoint = 1;
constexpr short kNSEventSubtypeTabletProximity = 2;

/* NSEventPhase */
constexpr NSUInteger_ kNSEventPhaseNone = 0;
constexpr NSUInteger_ kNSEventPhaseBegan = 1 << 0;
constexpr NSUInteger_ kNSEventPhaseEnded = 1 << 3;

/* NSPointingDeviceType */
constexpr NSUInteger_ kNSPointingDeviceTypeUnknown = 0;
constexpr NSUInteger_ kNSPointingDeviceTypePen = 1;
constexpr NSUInteger_ kNSPointingDeviceTypeCursor = 2;
constexpr NSUInteger_ kNSPointingDeviceTypeEraser = 3;

/* NSBitmapFormat */
constexpr NSUInteger_ kNSBitmapFormatAlphaFirst = 1 << 0;
constexpr NSUInteger_ kNSBitmapFormatFloatingPointSamples = 1 << 2;

/* NSAlertStyle y respuestas modales. */
constexpr NSUInteger_ kNSAlertStyleWarning = 0;
constexpr NSUInteger_ kNSAlertStyleInformational = 1;
constexpr NSUInteger_ kNSAlertStyleCritical = 2;
constexpr NSInteger_ kNSAlertSecondButtonReturn = 1001;

constexpr NSUInteger_ kNSApplicationTerminateCancel = 0;
constexpr NSUInteger_ kNSUTF8StringEncoding = 4;
constexpr NSInteger_ kNSNotFound = 0x7fffffffffffffffLL;

/** Ayuda: `@"texto"` autoliberado. */
id ns(const char *utf8)
{
  return ghost_objc::nsstring(utf8);
}

/** Ayuda: `[coleccion count]`. */
NSUInteger_ count_of(id collection)
{
  return collection ? msg<NSUInteger_>(collection, GHOST_SEL(count)) : 0;
}

/** Ayuda: `[array objectAtIndex:i]`. */
id object_at(id array, NSUInteger_ i)
{
  return msg<id>(array, GHOST_SEL(objectAtIndex:), i);
}

/* -------------------------------------------------------------------------
 * Accesos a propiedades de NSEvent.
 *
 * `ev_pressure(event)` en Objective-C es exactamente `[event pressure]`. Se envuelve cada
 * una en una funcion para escribir el tipo de retorno UNA sola vez y en un sitio donde
 * se puede revisar contra la cabecera del SDK: equivocarse en el tipo no da error, da
 * basura. Los tipos son los de `AppKit/NSEvent.h` del SDK 26.5.
 *
 * Ojo con los tres que tienen `getter=`: la propiedad se llama `enteringProximity`,
 * `ARepeat` y `directionInvertedFromDevice`, pero el SELECTOR es `isEnteringProximity`,
 * `isARepeat` e `isDirectionInvertedFromDevice`.
 */
inline id ev_window(id e) { return msg<id>(e, GHOST_SEL(window)); }
inline NSUInteger_ ev_type(id e) { return msg<NSUInteger_>(e, GHOST_SEL(type)); }
inline double ev_timestamp(id e) { return msg<double>(e, GHOST_SEL(timestamp)); }
inline CGPoint ev_locationInWindow(id e) { return msg<CGPoint>(e, GHOST_SEL(locationInWindow)); }
inline unsigned short ev_keyCode(id e) { return msg<unsigned short>(e, GHOST_SEL(keyCode)); }
inline NSUInteger_ ev_modifierFlags(id e)
{
  return msg<NSUInteger_>(e, GHOST_SEL(modifierFlags));
}
inline CGFloat ev_deltaX(id e) { return msg<CGFloat>(e, GHOST_SEL(deltaX)); }
inline CGFloat ev_deltaY(id e) { return msg<CGFloat>(e, GHOST_SEL(deltaY)); }
inline CGFloat ev_scrollingDeltaX(id e) { return msg<CGFloat>(e, GHOST_SEL(scrollingDeltaX)); }
inline CGFloat ev_scrollingDeltaY(id e) { return msg<CGFloat>(e, GHOST_SEL(scrollingDeltaY)); }
inline CGPoint ev_tilt(id e) { return msg<CGPoint>(e, GHOST_SEL(tilt)); }
inline float ev_pressure(id e) { return msg<float>(e, GHOST_SEL(pressure)); }
inline float ev_rotation(id e) { return msg<float>(e, GHOST_SEL(rotation)); }
inline CGFloat ev_magnification(id e) { return msg<CGFloat>(e, GHOST_SEL(magnification)); }
inline NSUInteger_ ev_pointingDeviceType(id e)
{
  return msg<NSUInteger_>(e, GHOST_SEL(pointingDeviceType));
}
inline NSInteger_ ev_buttonNumber(id e) { return msg<NSInteger_>(e, GHOST_SEL(buttonNumber)); }
inline short ev_subtype(id e) { return msg<short>(e, GHOST_SEL(subtype)); }
inline NSUInteger_ ev_phase(id e) { return msg<NSUInteger_>(e, GHOST_SEL(phase)); }
inline NSUInteger_ ev_momentumPhase(id e)
{
  return msg<NSUInteger_>(e, GHOST_SEL(momentumPhase));
}
inline bool ev_isEnteringProximity(id e)
{
  return msg<signed char>(e, GHOST_SEL(isEnteringProximity)) != 0;
}
inline bool ev_isARepeat(id e) { return msg<signed char>(e, GHOST_SEL(isARepeat)) != 0; }
inline bool ev_isDirectionInvertedFromDevice(id e)
{
  return msg<signed char>(e, GHOST_SEL(isDirectionInvertedFromDevice)) != 0;
}
inline id ev_characters(id e) { return msg<id>(e, GHOST_SEL(characters)); }
inline id ev_charactersIgnoringModifiers(id e)
{
  return msg<id>(e, GHOST_SEL(charactersIgnoringModifiers));
}

/* -------------------------------------------------------------------------
 * Apoyo del cuentagotas (`getPixelAtCursor`).
 *
 * El bloque solo puede capturar un puntero, asi que todo el estado va en esta struct de
 * la pila y el bloque recibe su direccion.
 */
struct SamplerState {
  float *r_color;
  id selected_color;
  bool completed;
  bool succeeded;
};

/** Lo que corria dentro del `dispatch_after` del original. */
void sampler_after(void *ctx)
{
  SamplerState *st = (SamplerState *)ctx;
  if (st->selected_color != nullptr) {
    id rgbColor = msg<id>(st->selected_color,
                          GHOST_SEL(colorUsingColorSpace:),
                          msg<id>(GHOST_CLS(NSColorSpace), GHOST_SEL(deviceRGBColorSpace)));
    if (rgbColor) {
      st->r_color[0] = msg<double>(rgbColor, GHOST_SEL(redComponent));
      st->r_color[1] = msg<double>(rgbColor, GHOST_SEL(greenComponent));
      st->r_color[2] = msg<double>(rgbColor, GHOST_SEL(blueComponent));
    }
    st->succeeded = true;
  }
  st->completed = true;
}

/** El cuerpo del bloque. Primer parametro: el propio bloque. Segundo: el NSColor. */
void sampler_handler(void *block, id selectedColor)
{
  SamplerState *st = (SamplerState *)ghost_objc::Block::context_of(block);
  /* El color lo entrega el muestreador autoliberado y se usa 0,1 s despues: hay que
   * retenerlo o para entonces puede estar libre. El original no tenia que hacerlo
   * porque el bloque capturaba el objeto y el compilador lo retenia por el. */
  st->selected_color = ghost_objc::retain(selectedColor);
  dispatch_after_f(dispatch_time(DISPATCH_TIME_NOW, (int64_t)(0.1 * NSEC_PER_SEC)),
                   dispatch_get_main_queue(),
                   st,
                   sampler_after);
}

/** Ayuda: `@[ a ]` y `@[ a, b, c ]`. */
id ns_array(id a, id b = nullptr, id c = nullptr)
{
  return msg<id>(GHOST_CLS(NSArray), GHOST_SEL(arrayWithObjects:), a, b, c, (id) nullptr);
}

}  // namespace

/* --------------------------------------------------------------------
 * Keymaps, mouse converters.
 */

static GHOST_TButton convertButton(int button)
{
  switch (button) {
    case 0:
      return GHOST_kButtonMaskLeft;
    case 1:
      return GHOST_kButtonMaskRight;
    case 2:
      return GHOST_kButtonMaskMiddle;
    case 3:
      return GHOST_kButtonMaskButton4;
    case 4:
      return GHOST_kButtonMaskButton5;
    case 5:
      return GHOST_kButtonMaskButton6;
    case 6:
      return GHOST_kButtonMaskButton7;
    default:
      return GHOST_kButtonMaskLeft;
  }
}

/**
 * Converts Mac raw-key codes (same for Cocoa & Carbon)
 * into GHOST key codes
 * \param rawCode: The raw physical key code
 * \param recvChar: the character ignoring modifiers (except for shift)
 * \return Ghost key code
 */
static GHOST_TKey convertKey(int rawCode, unichar recvChar)
{
  // printf("\nrecvchar %c 0x%x",recvChar,recvChar);
  switch (rawCode) {
    /* Physical key-codes: (not used due to map changes in international keyboards). */
#if 0
    case kVK_ANSI_A:    return GHOST_kKeyA;
    case kVK_ANSI_B:    return GHOST_kKeyB;
    case kVK_ANSI_C:    return GHOST_kKeyC;
    case kVK_ANSI_D:    return GHOST_kKeyD;
    case kVK_ANSI_E:    return GHOST_kKeyE;
    case kVK_ANSI_F:    return GHOST_kKeyF;
    case kVK_ANSI_G:    return GHOST_kKeyG;
    case kVK_ANSI_H:    return GHOST_kKeyH;
    case kVK_ANSI_I:    return GHOST_kKeyI;
    case kVK_ANSI_J:    return GHOST_kKeyJ;
    case kVK_ANSI_K:    return GHOST_kKeyK;
    case kVK_ANSI_L:    return GHOST_kKeyL;
    case kVK_ANSI_M:    return GHOST_kKeyM;
    case kVK_ANSI_N:    return GHOST_kKeyN;
    case kVK_ANSI_O:    return GHOST_kKeyO;
    case kVK_ANSI_P:    return GHOST_kKeyP;
    case kVK_ANSI_Q:    return GHOST_kKeyQ;
    case kVK_ANSI_R:    return GHOST_kKeyR;
    case kVK_ANSI_S:    return GHOST_kKeyS;
    case kVK_ANSI_T:    return GHOST_kKeyT;
    case kVK_ANSI_U:    return GHOST_kKeyU;
    case kVK_ANSI_V:    return GHOST_kKeyV;
    case kVK_ANSI_W:    return GHOST_kKeyW;
    case kVK_ANSI_X:    return GHOST_kKeyX;
    case kVK_ANSI_Y:    return GHOST_kKeyY;
    case kVK_ANSI_Z:    return GHOST_kKeyZ;
#endif
    /* Numbers keys: mapped to handle some international keyboard (e.g. French). */
    case kVK_ANSI_1:
      return GHOST_kKey1;
    case kVK_ANSI_2:
      return GHOST_kKey2;
    case kVK_ANSI_3:
      return GHOST_kKey3;
    case kVK_ANSI_4:
      return GHOST_kKey4;
    case kVK_ANSI_5:
      return GHOST_kKey5;
    case kVK_ANSI_6:
      return GHOST_kKey6;
    case kVK_ANSI_7:
      return GHOST_kKey7;
    case kVK_ANSI_8:
      return GHOST_kKey8;
    case kVK_ANSI_9:
      return GHOST_kKey9;
    case kVK_ANSI_0:
      return GHOST_kKey0;

    case kVK_ANSI_Keypad0:
      return GHOST_kKeyNumpad0;
    case kVK_ANSI_Keypad1:
      return GHOST_kKeyNumpad1;
    case kVK_ANSI_Keypad2:
      return GHOST_kKeyNumpad2;
    case kVK_ANSI_Keypad3:
      return GHOST_kKeyNumpad3;
    case kVK_ANSI_Keypad4:
      return GHOST_kKeyNumpad4;
    case kVK_ANSI_Keypad5:
      return GHOST_kKeyNumpad5;
    case kVK_ANSI_Keypad6:
      return GHOST_kKeyNumpad6;
    case kVK_ANSI_Keypad7:
      return GHOST_kKeyNumpad7;
    case kVK_ANSI_Keypad8:
      return GHOST_kKeyNumpad8;
    case kVK_ANSI_Keypad9:
      return GHOST_kKeyNumpad9;
    case kVK_ANSI_KeypadDecimal:
      return GHOST_kKeyNumpadPeriod;
    case kVK_ANSI_KeypadEnter:
      return GHOST_kKeyNumpadEnter;
    case kVK_ANSI_KeypadPlus:
      return GHOST_kKeyNumpadPlus;
    case kVK_ANSI_KeypadMinus:
      return GHOST_kKeyNumpadMinus;
    case kVK_ANSI_KeypadMultiply:
      return GHOST_kKeyNumpadAsterisk;
    case kVK_ANSI_KeypadDivide:
      return GHOST_kKeyNumpadSlash;
    case kVK_ANSI_KeypadClear:
      return GHOST_kKeyUnknown;

    case kVK_F1:
      return GHOST_kKeyF1;
    case kVK_F2:
      return GHOST_kKeyF2;
    case kVK_F3:
      return GHOST_kKeyF3;
    case kVK_F4:
      return GHOST_kKeyF4;
    case kVK_F5:
      return GHOST_kKeyF5;
    case kVK_F6:
      return GHOST_kKeyF6;
    case kVK_F7:
      return GHOST_kKeyF7;
    case kVK_F8:
      return GHOST_kKeyF8;
    case kVK_F9:
      return GHOST_kKeyF9;
    case kVK_F10:
      return GHOST_kKeyF10;
    case kVK_F11:
      return GHOST_kKeyF11;
    case kVK_F12:
      return GHOST_kKeyF12;
    case kVK_F13:
      return GHOST_kKeyF13;
    case kVK_F14:
      return GHOST_kKeyF14;
    case kVK_F15:
      return GHOST_kKeyF15;
    case kVK_F16:
      return GHOST_kKeyF16;
    case kVK_F17:
      return GHOST_kKeyF17;
    case kVK_F18:
      return GHOST_kKeyF18;
    case kVK_F19:
      return GHOST_kKeyF19;
    case kVK_F20:
      return GHOST_kKeyF20;

    case kVK_UpArrow:
      return GHOST_kKeyUpArrow;
    case kVK_DownArrow:
      return GHOST_kKeyDownArrow;
    case kVK_LeftArrow:
      return GHOST_kKeyLeftArrow;
    case kVK_RightArrow:
      return GHOST_kKeyRightArrow;

    case kVK_Return:
      return GHOST_kKeyEnter;
    case kVK_Delete:
      return GHOST_kKeyBackSpace;
    case kVK_ForwardDelete:
      return GHOST_kKeyDelete;
    case kVK_Escape:
      return GHOST_kKeyEsc;
    case kVK_Tab:
      return GHOST_kKeyTab;
    case kVK_Space:
      return GHOST_kKeySpace;

    case kVK_Home:
      return GHOST_kKeyHome;
    case kVK_End:
      return GHOST_kKeyEnd;
    case kVK_PageUp:
      return GHOST_kKeyUpPage;
    case kVK_PageDown:
      return GHOST_kKeyDownPage;
#if 0
    /* These constants with "ANSI" in the name are labeled according to the key position on an
     * ANSI-standard US keyboard. Therefore they may not match the physical key label on other
     * keyboard layouts. */
    case kVK_ANSI_Minus:        return GHOST_kKeyMinus;
    case kVK_ANSI_Equal:        return GHOST_kKeyEqual;
    case kVK_ANSI_Comma:        return GHOST_kKeyComma;
    case kVK_ANSI_Period:       return GHOST_kKeyPeriod;
    case kVK_ANSI_Slash:        return GHOST_kKeySlash;
    case kVK_ANSI_Semicolon:    return GHOST_kKeySemicolon;
    case kVK_ANSI_Quote:        return GHOST_kKeyQuote;
    case kVK_ANSI_Backslash:    return GHOST_kKeyBackslash;
    case kVK_ANSI_LeftBracket:  return GHOST_kKeyLeftBracket;
    case kVK_ANSI_RightBracket: return GHOST_kKeyRightBracket;
    case kVK_ANSI_Grave:        return GHOST_kKeyAccentGrave;
    case kVK_ISO_Section:       return GHOST_kKeyUnknown;
#endif
    case kVK_VolumeUp:
    case kVK_VolumeDown:
    case kVK_Mute:
      return GHOST_kKeyUnknown;

    default: {
      /* Alphanumerical or punctuation key that is remappable in international keyboards. */
      if ((recvChar >= 'A') && (recvChar <= 'Z')) {
        return (GHOST_TKey)(recvChar - 'A' + GHOST_kKeyA);
      }

      if ((recvChar >= 'a') && (recvChar <= 'z')) {
        return (GHOST_TKey)(recvChar - 'a' + GHOST_kKeyA);
      }
      else {
        /* Leopard and Snow Leopard 64bit compatible API. */
        const TISInputSourceRef kbdTISHandle = TISCopyCurrentKeyboardLayoutInputSource();
        /* The keyboard layout. */
        const CFDataRef uchrHandle = static_cast<CFDataRef>(
            TISGetInputSourceProperty(kbdTISHandle, kTISPropertyUnicodeKeyLayoutData));
        CFRelease(kbdTISHandle);

        /* Get actual character value of the "remappable" keys in international keyboards,
         * if keyboard layout is not correctly reported (e.g. some non Apple keyboards in Tiger),
         * then fall back on using the received #charactersIgnoringModifiers. */
        if (uchrHandle) {
          UInt32 deadKeyState = 0;
          UniCharCount actualStrLength = 0;

          UCKeyTranslate((UCKeyboardLayout *)CFDataGetBytePtr(uchrHandle),
                         rawCode,
                         kUCKeyActionDown,
                         0,
                         LMGetKbdType(),
                         kUCKeyTranslateNoDeadKeysMask,
                         &deadKeyState,
                         1,
                         &actualStrLength,
                         &recvChar);
        }

        switch (recvChar) {
          case '-':
            return GHOST_kKeyMinus;
          case '+':
            return GHOST_kKeyPlus;
          case '=':
            return GHOST_kKeyEqual;
          case ',':
            return GHOST_kKeyComma;
          case '.':
            return GHOST_kKeyPeriod;
          case '/':
            return GHOST_kKeySlash;
          case ';':
            return GHOST_kKeySemicolon;
          case '\'':
            return GHOST_kKeyQuote;
          case '\\':
            return GHOST_kKeyBackslash;
          case '[':
            return GHOST_kKeyLeftBracket;
          case ']':
            return GHOST_kKeyRightBracket;
          case '`':
          case '<': /* The position of '`' is equivalent to this symbol in the French layout. */
            return GHOST_kKeyAccentGrave;
          default:
            return GHOST_kKeyUnknown;
        }
      }
    }
  }
  return GHOST_kKeyUnknown;
}

/* --------------------------------------------------------------------
 * Utility functions.
 */

#define FIRSTFILEBUFLG 512
static bool g_hasFirstFile = false;
static char g_firstFileBuf[FIRSTFILEBUFLG];

/* TODO: Need to investigate this.
 * Function called too early in creator.c to have g_hasFirstFile == true */
extern "C" int GHOST_HACK_getFirstFile(char buf[FIRSTFILEBUFLG])
{
  if (g_hasFirstFile) {
    memcpy(buf, g_firstFileBuf, FIRSTFILEBUFLG);
    buf[FIRSTFILEBUFLG - 1] = '\0';
    return 1;
  }
  return 0;
}
/* --------------------------------------------------------------------
 * Cocoa objects.
 */

namespace {

/**
 * CocoaAppDelegate
 * ObjC object to capture applicationShouldTerminate, and send quit event
 *
 * Se fabrica en tiempo de ejecucion. Las codificaciones de tipo las da el runtime
 * (superclase NSObject + protocolo NSApplicationDelegate); solo dos son propias.
 */

const char *const kIvarSystem = "m_systemCocoa";

GHOST_SystemCocoa *system_of(id self)
{
  return ghost_objc::ivar_get<GHOST_SystemCocoa *>(self, kIvarSystem);
}

void imp_app_dealloc(id self, SEL _cmd)
{
  {
    ghost_objc::AutoreleasePool pool;
    id center = msg<id>(GHOST_CLS(NSNotificationCenter), GHOST_SEL(defaultCenter));
    msg<void>(center,
              GHOST_SEL(removeObserver:name:object:),
              self,
              NSWindowWillCloseNotification,
              (id) nullptr);
  }
  /* El original tenia `[super dealloc]` DENTRO del `@autoreleasepool`. Aqui va fuera, a
   * proposito: drenar un pool despues de destruir el objeto que lo contenia no aporta
   * nada y el orden correcto es soltar el observador, cerrar el pool y luego destruir. */
  ghost_objc::msg_super<void>(self, class_getSuperclass(object_getClass(self)), _cmd);
}

void imp_applicationDidFinishLaunching(id self, SEL, id /*aNotification*/)
{
  if (system_of(self)->m_windowFocus) {
    /* Raise application to front, convenient when starting from the terminal
     * and important for launching the animation player. we call this after the
     * application finishes launching, as doing it earlier can make us end up
     * with a front-most window but an inactive application. */
    msg<void>(NSApp, GHOST_SEL(activateIgnoringOtherApps:), (signed char)1);
  }

  msg<void>(GHOST_CLS(NSEvent), GHOST_SEL(setMouseCoalescingEnabled:), (signed char)0);
}

signed char imp_application_openFile(id self, SEL, id /*theApplication*/, id filename)
{
  return system_of(self)->handleOpenDocumentRequest(filename) ? 1 : 0;
}

NSUInteger_ imp_applicationShouldTerminate(id self, SEL, id /*sender*/)
{
  /* TODO: implement graceful termination through Cocoa mechanism
   * to avoid session log off to be canceled. */
  /* Note that Command-Q is already handled by key-handler. */
  system_of(self)->handleQuitRequest();
  return kNSApplicationTerminateCancel;
}

/* To avoid canceling a log off process, we must use Cocoa termination process
 * And this function is the only chance to perform clean up
 * So WM_exit needs to be called directly, as the event loop will never run before termination. */
void imp_applicationWillTerminate(id, SEL, id /*aNotification*/)
{
#if 0
  WM_exit(C, EXIT_SUCCESS);
#endif
}

void imp_applicationWillBecomeActive(id self, SEL, id /*aNotification*/)
{
  system_of(self)->handleApplicationBecomeActiveEvent();
}

void imp_toggleFullScreen(id, SEL, id /*notification*/) {}

/* The purpose of this function is to make sure closing "About" window does not
 * leave Blender with no key windows. This is needed due to a custom event loop
 * nature of the application: for some reason only using [NSApp run] will ensure
 * correct behavior in this case.
 *
 * This is similar to an issue solved in SDL:
 *   https://bugzilla.libsdl.org/show_bug.cgi?id=1825
 *
 * Our solution is different, since we want Blender to keep track of what is
 * the key window during normal operation. In order to do so we exploit the
 * fact that "About" window is never in the orderedWindows array: we only force
 * key window from here if the closing one is not in the orderedWindows. This
 * saves lack of key windows when closing "About", but does not interfere with
 * Blender's window manager when closing Blender's windows.
 *
 * NOTE: It also receives notifiers when menus are closed on macOS 14.
 * Presumably it considers menus to be windows. */
void imp_windowWillClose(id, SEL, id notification)
{
  ghost_objc::AutoreleasePool pool;
  id closing_window = msg<id>(notification, GHOST_SEL(object));

  if (!msg<signed char>(closing_window, GHOST_SEL(isKeyWindow))) {
    /* If the window wasn't key then its either none of the windows are key or another window
     * is a key. The former situation is a bit strange, but probably forcing a key window is not
     * something desirable. The latter situation is when we definitely do not want to change the
     * key window.
     *
     * Ignoring non-key windows also avoids the code which ensures ordering below from running
     * when the notifier is received for menus on macOS 14. */
    return;
  }

  id ordered = msg<id>(NSApp, GHOST_SEL(orderedWindows));
  const NSInteger_ index = msg<NSInteger_>(ordered, GHOST_SEL(indexOfObject:), closing_window);
  if (index != kNSNotFound) {
    return;
  }
  /* Find first suitable window from the current space.
   * El original usaba enumeracion rapida (`for (x in coleccion)`), que es sintaxis de
   * Objective-C. Sobre un NSArray el recorrido por indice es exactamente equivalente. */
  const NSUInteger_ n_ordered = count_of(ordered);
  for (NSUInteger_ i = 0; i < n_ordered; i++) {
    id current_window = object_at(ordered, i);
    if (current_window == closing_window) {
      continue;
    }
    if (msg<signed char>(current_window, GHOST_SEL(isOnActiveSpace)) &&
        msg<signed char>(current_window, GHOST_SEL(canBecomeKeyWindow)))
    {
      msg<void>(current_window, GHOST_SEL(makeKeyAndOrderFront:), (id) nullptr);
      return;
    }
  }
  /* If that didn't find any windows, we try to find any suitable window of the application. */
  id numbers = msg<id>(
      GHOST_CLS(NSWindow), GHOST_SEL(windowNumbersWithOptions:), (NSUInteger_)0);
  const NSUInteger_ n_numbers = count_of(numbers);
  for (NSUInteger_ i = 0; i < n_numbers; i++) {
    id window_number = object_at(numbers, i);
    id current_window = msg<id>(NSApp,
                                GHOST_SEL(windowWithWindowNumber:),
                                msg<NSInteger_>(window_number, GHOST_SEL(integerValue)));
    if (current_window == closing_window) {
      continue;
    }
    if (msg<signed char>(current_window, GHOST_SEL(canBecomeKeyWindow))) {
      msg<void>(current_window, GHOST_SEL(makeKeyAndOrderFront:), (id) nullptr);
      return;
    }
  }
}

/* Explicitly opt-in to the secure coding for the restorable state.
 *
 * This is something that only has affect on macOS 12+, and is implicitly
 * enabled on macOS 14.
 *
 * For the details see
 *   https://sector7.computest.nl/post/2022-08-process-injection-breaking-all-macos-security-layers-with-a-single-vulnerability/
 */
signed char imp_applicationSupportsSecureRestorableState(id, SEL, id /*app*/)
{
  return 1; /* YES */
}

Class build_app_delegate_class()
{
  ghost_objc::ClassBuilder b("CocoaAppDelegate", "NSObject");
  b.protocol("NSApplicationDelegate");
  b.ivar(kIvarSystem, sizeof(void *), 3, "^v");
  b.method("dealloc", (IMP)imp_app_dealloc);
  b.method("applicationDidFinishLaunching:", (IMP)imp_applicationDidFinishLaunching);
  b.method("application:openFile:", (IMP)imp_application_openFile);
  b.method("applicationShouldTerminate:", (IMP)imp_applicationShouldTerminate);
  b.method("applicationWillTerminate:", (IMP)imp_applicationWillTerminate);
  b.method("applicationWillBecomeActive:", (IMP)imp_applicationWillBecomeActive);
  b.method("applicationSupportsSecureRestorableState:",
           (IMP)imp_applicationSupportsSecureRestorableState);
  /* Los dos PROPIOS: el runtime no puede conocerlos porque no existen en ningun otro
   * sitio. `v@:@` = void, self, _cmd, un objeto; es la forma de todos los manejadores
   * de notificacion. */
  b.method("toggleFullScreen:", (IMP)imp_toggleFullScreen, "v@:@");
  b.method("windowWillClose:", (IMP)imp_windowWillClose, "v@:@");
  return b.finish();
}

Class app_delegate_class()
{
  static Class cls = build_app_delegate_class();
  return cls;
}

/** Equivale a `[[CocoaAppDelegate alloc] initWithSystemCocoa:system]`. */
id app_delegate_create(GHOST_SystemCocoa *system)
{
  id delegate = msg<id>(msg<id>((id)app_delegate_class(), GHOST_SEL(alloc)), GHOST_SEL(init));
  if (!delegate) {
    return nullptr;
  }
  id center = msg<id>(GHOST_CLS(NSNotificationCenter), GHOST_SEL(defaultCenter));
  msg<void>(center,
            GHOST_SEL(addObserver:selector:name:object:),
            delegate,
            ghost_objc::sel("windowWillClose:"),
            NSWindowWillCloseNotification,
            (id) nullptr);
  ghost_objc::ivar_set<GHOST_SystemCocoa *>(delegate, kIvarSystem, system);
  return delegate;
}

}  // namespace

/* --------------------------------------------------------------------
 * Initialization / Finalization.
 */

GHOST_SystemCocoa::GHOST_SystemCocoa()
{
  m_modifierMask = 0;
  m_outsideLoopEventProcessed = false;
  m_needDelayedApplicationBecomeActiveEventProcessing = false;

  m_ignoreWindowSizedMessages = false;
  m_ignoreMomentumScroll = false;
  m_multiTouchScroll = false;
  m_last_warp_timestamp = 0;
}

GHOST_SystemCocoa::~GHOST_SystemCocoa()
{
  /* The application delegate integrates the Cocoa application with the GHOST system.
   *
   * Since the GHOST system is about to be fully destroyed release the application delegate as
   * well, so it does not point back to a freed system, forcing the delegate to be created with the
   * new GHOST system in init(). */
  {
    ghost_objc::AutoreleasePool pool;
    id appDelegate = msg<id>(NSApp, GHOST_SEL(delegate));
    if (appDelegate) {
      msg<void>(NSApp, GHOST_SEL(setDelegate:), (id) nullptr);
      ghost_objc::release(appDelegate);
    }
  }
}

GHOST_TSuccess GHOST_SystemCocoa::init()
{
  GHOST_TSuccess success = GHOST_System::init();
  if (success) {

#ifdef WITH_INPUT_NDOF
    m_ndofManager = new GHOST_NDOFManagerCocoa(*this);
#endif

    // ProcessSerialNumber psn;

    /* Carbon stuff to move window & menu to foreground. */
#if 0
    if (!GetCurrentProcess(&psn)) {
      TransformProcessType(&psn, kProcessTransformToForegroundApplication);
      SetFrontProcess(&psn);
    }
#endif

    {
      ghost_objc::AutoreleasePool pool;
      msg<id>(GHOST_CLS(NSApplication), GHOST_SEL(sharedApplication)); /* initializes `NSApp`. */

      if (msg<id>(NSApp, GHOST_SEL(mainMenu)) == nullptr) {
        /* `addItemWithTitle:action:keyEquivalent:` devuelve el NSMenuItem creado. Los
         * `@selector(...)` pasan a `ghost_objc::sel(...)`, que es literalmente lo mismo:
         * `@selector` es azucar sintactico de `sel_registerName`. */
        static const SEL sel_addItem = ghost_objc::sel("addItemWithTitle:action:keyEquivalent:");

        id mainMenubar = ghost_objc::alloc_init("NSMenu");
        id menuItem;

        /* Create the application menu. */
        id appMenu = msg<id>(msg<id>(GHOST_CLS(NSMenu), GHOST_SEL(alloc)),
                             GHOST_SEL(initWithTitle:),
                             ns("Blender"));

        msg<id>(appMenu,
                sel_addItem,
                ns("About Blender"),
                ghost_objc::sel("orderFrontStandardAboutPanel:"),
                ns(""));
        msg<void>(appMenu,
                  GHOST_SEL(addItem:),
                  msg<id>(GHOST_CLS(NSMenuItem), GHOST_SEL(separatorItem)));

        menuItem = msg<id>(
            appMenu, sel_addItem, ns("Hide Blender"), ghost_objc::sel("hide:"), ns("h"));
        msg<void>(menuItem, GHOST_SEL(setKeyEquivalentModifierMask:), kNSEventModifierFlagCommand);

        menuItem = msg<id>(appMenu,
                           sel_addItem,
                           ns("Hide Others"),
                           ghost_objc::sel("hideOtherApplications:"),
                           ns("h"));
        msg<void>(menuItem,
                  GHOST_SEL(setKeyEquivalentModifierMask:),
                  (NSUInteger_)(kNSEventModifierFlagOption | kNSEventModifierFlagCommand));

        msg<id>(appMenu,
                sel_addItem,
                ns("Show All"),
                ghost_objc::sel("unhideAllApplications:"),
                ns(""));

        menuItem = msg<id>(
            appMenu, sel_addItem, ns("Quit Blender"), ghost_objc::sel("terminate:"), ns("q"));
        msg<void>(menuItem, GHOST_SEL(setKeyEquivalentModifierMask:), kNSEventModifierFlagCommand);

        menuItem = ghost_objc::alloc_init("NSMenuItem");
        msg<void>(menuItem, GHOST_SEL(setSubmenu:), appMenu);

        msg<void>(mainMenubar, GHOST_SEL(addItem:), menuItem);
        ghost_objc::release(menuItem);
        ghost_objc::release(appMenu);

        /* Create the window menu. */
        id windowMenu = msg<id>(msg<id>(GHOST_CLS(NSMenu), GHOST_SEL(alloc)),
                                GHOST_SEL(initWithTitle:),
                                ns("Window"));

        menuItem = msg<id>(windowMenu,
                           sel_addItem,
                           ns("Minimize"),
                           ghost_objc::sel("performMiniaturize:"),
                           ns("m"));
        msg<void>(menuItem, GHOST_SEL(setKeyEquivalentModifierMask:), kNSEventModifierFlagCommand);

        msg<id>(windowMenu, sel_addItem, ns("Zoom"), ghost_objc::sel("performZoom:"), ns(""));

        menuItem = msg<id>(windowMenu,
                           sel_addItem,
                           ns("Enter Full Screen"),
                           ghost_objc::sel("toggleFullScreen:"),
                           ns("f"));
        msg<void>(menuItem,
                  GHOST_SEL(setKeyEquivalentModifierMask:),
                  (NSUInteger_)(kNSEventModifierFlagControl | kNSEventModifierFlagCommand));

        menuItem = msg<id>(
            windowMenu, sel_addItem, ns("Close"), ghost_objc::sel("performClose:"), ns("w"));
        msg<void>(menuItem, GHOST_SEL(setKeyEquivalentModifierMask:), kNSEventModifierFlagCommand);

        menuItem = ghost_objc::alloc_init("NSMenuItem");
        msg<void>(menuItem, GHOST_SEL(setSubmenu:), windowMenu);

        msg<void>(mainMenubar, GHOST_SEL(addItem:), menuItem);
        ghost_objc::release(menuItem);

        msg<void>(NSApp, GHOST_SEL(setMainMenu:), mainMenubar);
        msg<void>(NSApp, GHOST_SEL(setWindowsMenu:), windowMenu);
        ghost_objc::release(windowMenu);
      }

      if (msg<id>(NSApp, GHOST_SEL(delegate)) == nullptr) {
        msg<void>(NSApp, GHOST_SEL(setDelegate:), app_delegate_create(this));
      }

      /* AppKit provides automatic window tabbing. Blender is a single-tabbed
       * application without a macOS tab bar, and should explicitly opt-out of this.
       * This is also controlled by the macOS user default #NSWindowTabbingEnabled. */
      msg<void>(GHOST_CLS(NSWindow), GHOST_SEL(setAllowsAutomaticWindowTabbing:), (signed char)0);

      msg<void>(NSApp, GHOST_SEL(finishLaunching));
    }
  }
  return success;
}

/* --------------------------------------------------------------------
 * Window management.
 */

uint64_t GHOST_SystemCocoa::getMilliSeconds() const
{
  /* For comparing to NSEvent timestamp, this particular API function matches. */
  return (uint64_t)(msg<double>(msg<id>(GHOST_CLS(NSProcessInfo), GHOST_SEL(processInfo)),
                                GHOST_SEL(systemUptime)) *
                    1000);
}

uint8_t GHOST_SystemCocoa::getNumDisplays() const
{
  /* Note that OS X supports monitor hot plug.
   * We do not support multiple monitors at the moment. */
  ghost_objc::AutoreleasePool pool;
  return count_of(msg<id>(GHOST_CLS(NSScreen), GHOST_SEL(screens)));
}

void GHOST_SystemCocoa::getMainDisplayDimensions(uint32_t &width, uint32_t &height) const
{
  ghost_objc::AutoreleasePool pool;
  /* Get visible frame, that is frame excluding dock and top menu bar. */
  const CGRect frame = msg<CGRect>(msg<id>(GHOST_CLS(NSScreen), GHOST_SEL(mainScreen)),
                                   GHOST_SEL(visibleFrame));

  /* Returns max window contents (excluding title bar...). */
  const CGRect contentRect = msg<CGRect>(
      GHOST_CLS(NSWindow),
      GHOST_SEL(contentRectForFrameRect:styleMask:),
      frame,
      (NSUInteger_)(kNSWindowStyleMaskTitled | kNSWindowStyleMaskClosable |
                    kNSWindowStyleMaskMiniaturizable));

  width = contentRect.size.width;
  height = contentRect.size.height;
}

void GHOST_SystemCocoa::getAllDisplayDimensions(uint32_t &width, uint32_t &height) const
{
  /* TODO! */
  getMainDisplayDimensions(width, height);
}

GHOST_IWindow *GHOST_SystemCocoa::createWindow(const char *title,
                                               int32_t left,
                                               int32_t top,
                                               uint32_t width,
                                               uint32_t height,
                                               GHOST_TWindowState state,
                                               GHOST_GPUSettings gpuSettings,
                                               const bool /*exclusive*/,
                                               const bool is_dialog,
                                               const GHOST_IWindow *parentWindow)
{
  GHOST_IWindow *window = nullptr;
  {
    ghost_objc::AutoreleasePool pool;
    /* Get the available rect for including window contents. */
    const CGRect frame = msg<CGRect>(msg<id>(GHOST_CLS(NSScreen), GHOST_SEL(mainScreen)),
                                     GHOST_SEL(visibleFrame));
    const CGRect contentRect = msg<CGRect>(
        GHOST_CLS(NSWindow),
        GHOST_SEL(contentRectForFrameRect:styleMask:),
        frame,
        (NSUInteger_)(kNSWindowStyleMaskTitled | kNSWindowStyleMaskClosable |
                      kNSWindowStyleMaskMiniaturizable));

    int32_t bottom = (contentRect.size.height - 1) - height - top;

    /* Ensures window top left is inside this available rect. */
    left = left > contentRect.origin.x ? left : contentRect.origin.x;
    /* Add `contentRect.origin.y` to respect dock-size. */
    bottom = bottom > contentRect.origin.y ? bottom + contentRect.origin.y : contentRect.origin.y;

    window = new GHOST_WindowCocoa(this,
                                   title,
                                   left,
                                   bottom,
                                   width,
                                   height,
                                   state,
                                   gpuSettings.context_type,
                                   gpuSettings.flags & GHOST_gpuStereoVisual,
                                   gpuSettings.flags & GHOST_gpuDebugContext,
                                   is_dialog,
                                   (GHOST_WindowCocoa *)parentWindow,
                                   gpuSettings.preferred_device);

    if (window->getValid()) {
      /* Store the pointer to the window. */
      GHOST_ASSERT(m_windowManager, "m_windowManager not initialized");
      m_windowManager->addWindow(window);
      m_windowManager->setActiveWindow(window);
      /* Need to tell window manager the new window is the active one
       * (Cocoa does not send the event activate upon window creation). */
      pushEvent(new GHOST_Event(getMilliSeconds(), GHOST_kEventWindowActivate, window));
      pushEvent(new GHOST_Event(getMilliSeconds(), GHOST_kEventWindowSize, window));
    }
    else {
      GHOST_PRINT("GHOST_SystemCocoa::createWindow(): window invalid\n");
      delete window;
      window = nullptr;
    }
  }
  return window;
}

/**
 * Create a new off-screen context.
 * Never explicitly delete the context, use #disposeContext() instead.
 * \return The new context (or 0 if creation failed).
 */
GHOST_IContext *GHOST_SystemCocoa::createOffscreenContext(GHOST_GPUSettings gpuSettings)
{
  const bool debug_context = (gpuSettings.flags & GHOST_gpuDebugContext) != 0;

  switch (gpuSettings.context_type) {
#ifdef WITH_VULKAN_BACKEND
    case GHOST_kDrawingContextTypeVulkan: {
      GHOST_Context *context = new GHOST_ContextVK(
          false, nullptr, 1, 2, debug_context, gpuSettings.preferred_device);
      if (context->initializeDrawingContext()) {
        return context;
      }
      delete context;
      return nullptr;
    }
#endif

#ifdef WITH_METAL_BACKEND
    case GHOST_kDrawingContextTypeMetal: {
      /* TODO(fclem): Remove OpenGL support and rename context to ContextMTL */
      GHOST_Context *context = new GHOST_ContextCGL(false, nullptr, nullptr, debug_context);
      if (context->initializeDrawingContext()) {
        return context;
      }
      delete context;
      return nullptr;
    }
#endif

    default:
      /* Unsupported backend. */
      return nullptr;
  }
}

/**
 * Dispose of a context.
 * \param context: Pointer to the context to be disposed.
 * \return Indication of success.
 */
GHOST_TSuccess GHOST_SystemCocoa::disposeContext(GHOST_IContext *context)
{
  delete context;

  return GHOST_kSuccess;
}

GHOST_IWindow *GHOST_SystemCocoa::getWindowUnderCursor(int32_t x, int32_t y)
{
  const CGPoint scr_co = CGPointMake(x, y);

  ghost_objc::AutoreleasePool pool;
  const NSInteger_ windowNumberAtPoint = msg<NSInteger_>(
      GHOST_CLS(NSWindow),
      GHOST_SEL(windowNumberAtPoint:belowWindowWithWindowNumber:),
      scr_co,
      (NSInteger_)0);
  id nswindow = msg<id>(NSApp, GHOST_SEL(windowWithWindowNumber:), windowNumberAtPoint);

  if (nswindow == nullptr) {
    return nullptr;
  }

  return m_windowManager->getWindowAssociatedWithOSWindow((const void *)nswindow);
}

/**
 * \note returns coordinates in Cocoa screen coordinates.
 */
GHOST_TSuccess GHOST_SystemCocoa::getCursorPosition(int32_t &x, int32_t &y) const
{
  const CGPoint mouseLoc = msg<CGPoint>(GHOST_CLS(NSEvent), GHOST_SEL(mouseLocation));

  /* Returns the mouse location in screen coordinates. */
  x = int32_t(mouseLoc.x);
  y = int32_t(mouseLoc.y);
  return GHOST_kSuccess;
}

/**
 * \note expect Cocoa screen coordinates.
 */
GHOST_TSuccess GHOST_SystemCocoa::setCursorPosition(int32_t x, int32_t y)
{
  GHOST_WindowCocoa *window = (GHOST_WindowCocoa *)m_windowManager->getActiveWindow();
  if (!window) {
    return GHOST_kFailure;
  }

  /* Cursor and mouse dissociation placed here not to interfere with continuous grab
   * (in cont. grab setMouseCursorPosition is directly called). */
  CGAssociateMouseAndMouseCursorPosition(false);
  setMouseCursorPosition(x, y);
  CGAssociateMouseAndMouseCursorPosition(true);

  /* Force mouse move event (not pushed by Cocoa). */
  pushEvent(new GHOST_EventCursor(
      getMilliSeconds(), GHOST_kEventCursorMove, window, x, y, window->GetCocoaTabletData()));
  m_outsideLoopEventProcessed = true;

  return GHOST_kSuccess;
}

GHOST_TSuccess GHOST_SystemCocoa::getPixelAtCursor(float r_color[3]) const
{
  ghost_objc::AutoreleasePool pool;

  /* `showSamplerWithSelectionHandler:` SOLO acepta un bloque: no tiene variante con
   * selector ni con puntero a funcion. Es el unico sitio de `intern/ghost` donde hace
   * falta uno, y se fabrica a mano desde C++ estandar (ver `ghost_objc::Block`), sin
   * activar `-fblocks`.
   *
   * Dos diferencias con el original, ninguna observable:
   *
   *   - El original usaba `__block BOOL` (extension de Clang) para que el bloque
   *     escribiera en variables de la pila. Aqui el bloque captura UN PUNTERO a una
   *     struct de la pila y escribe a traves de el: mismo efecto, captura POD, y sin
   *     necesidad de las ayudas de copia que exige `__block`.
   *   - El bloque INTERNO de `dispatch_after` se sustituye por `dispatch_after_f`, que
   *     es la variante con puntero a funcion de la misma API. Asi queda un solo bloque
   *     en todo el arbol en vez de dos anidados.
   */
  SamplerState st{r_color, nullptr, false, false};

  id sampler = ghost_objc::alloc_init("NSColorSampler");
  ghost_objc::Block handler((ghost_objc::Block::Invoke)sampler_handler, &st);
  msg<void>(sampler, GHOST_SEL(showSamplerWithSelectionHandler:), handler.get());

  while (!st.completed) {
    msg<void>(msg<id>(GHOST_CLS(NSRunLoop), GHOST_SEL(currentRunLoop)),
              GHOST_SEL(runMode:beforeDate:),
              NSDefaultRunLoopMode,
              msg<id>(GHOST_CLS(NSDate), GHOST_SEL(dateWithTimeIntervalSinceNow:), (double)0.05));
  }

  ghost_objc::release(st.selected_color);
  ghost_objc::release(sampler);
  return st.succeeded ? GHOST_kSuccess : GHOST_kFailure;
}

GHOST_TSuccess GHOST_SystemCocoa::setMouseCursorPosition(int32_t x, int32_t y)
{
  float xf = float(x), yf = float(y);
  GHOST_WindowCocoa *window = (GHOST_WindowCocoa *)m_windowManager->getActiveWindow();
  if (!window) {
    return GHOST_kFailure;
  }

  {
    ghost_objc::AutoreleasePool pool;
    id windowScreen = reinterpret_cast<id>(window->getScreen());
    const CGRect screenRect = msg<CGRect>(windowScreen, GHOST_SEL(frame));

    /* Set position relative to current screen. */
    xf -= screenRect.origin.x;
    yf -= screenRect.origin.y;

    /* Quartz Display Services uses the old coordinates (top left origin). */
    yf = screenRect.size.height - yf;

    CGDisplayMoveCursorToPoint(
        (CGDirectDisplayID)msg<unsigned int>(
            msg<id>(msg<id>(windowScreen, GHOST_SEL(deviceDescription)),
                    GHOST_SEL(objectForKey:),
                    ns("NSScreenNumber")),
            GHOST_SEL(unsignedIntValue)),
        CGPointMake(xf, yf));

    /* See https://stackoverflow.com/a/17559012. By default, hardware events
     * will be suppressed for 500ms after a synthetic mouse event. For unknown
     * reasons CGEventSourceSetLocalEventsSuppressionInterval does not work,
     * however calling CGAssociateMouseAndMouseCursorPosition also removes the
     * delay, even if this is undocumented. */
    CGAssociateMouseAndMouseCursorPosition(true);
  }
  return GHOST_kSuccess;
}

GHOST_TSuccess GHOST_SystemCocoa::getModifierKeys(GHOST_ModifierKeys &keys) const
{
  keys.set(GHOST_kModifierKeyLeftOS, (m_modifierMask & kNSEventModifierFlagCommand) ? true : false);
  keys.set(GHOST_kModifierKeyLeftAlt, (m_modifierMask & kNSEventModifierFlagOption) ? true : false);
  keys.set(GHOST_kModifierKeyLeftShift,
           (m_modifierMask & kNSEventModifierFlagShift) ? true : false);
  keys.set(GHOST_kModifierKeyLeftControl,
           (m_modifierMask & kNSEventModifierFlagControl) ? true : false);

  return GHOST_kSuccess;
}

GHOST_TSuccess GHOST_SystemCocoa::getButtons(GHOST_Buttons &buttons) const
{
  const UInt32 button_state = GetCurrentEventButtonState();

  buttons.clear();
  buttons.set(GHOST_kButtonMaskLeft, button_state & (1 << 0));
  buttons.set(GHOST_kButtonMaskRight, button_state & (1 << 1));
  buttons.set(GHOST_kButtonMaskMiddle, button_state & (1 << 2));
  buttons.set(GHOST_kButtonMaskButton4, button_state & (1 << 3));
  buttons.set(GHOST_kButtonMaskButton5, button_state & (1 << 4));
  return GHOST_kSuccess;
}

GHOST_TCapabilityFlag GHOST_SystemCocoa::getCapabilities() const
{
  return GHOST_TCapabilityFlag(
      GHOST_CAPABILITY_FLAG_ALL &
      ~(
          /* Cocoa has no support for a primary selection clipboard. */
          GHOST_kCapabilityPrimaryClipboard |
          /* Cocoa doesn't define a Hyper modifier key,
           * it's possible another modifier could be optionally used in it's place. */
          GHOST_kCapabilityKeyboardHyperKey));
}

/* --------------------------------------------------------------------
 * Event handlers.
 */

/**
 * The event queue polling function
 */
bool GHOST_SystemCocoa::processEvents(bool /*waitForEvent*/)
{
  bool anyProcessed = false;
  id event;

  /* TODO: implement timer? */
#if 0
  do {
    GHOST_TimerManager* timerMgr = getTimerManager();

    if (waitForEvent) {
      uint64_t next = timerMgr->nextFireTime();
      double timeOut;

      if (next == GHOST_kFireTimeNever) {
        timeOut = kEventDurationForever;
      }
      else {
        timeOut = (double)(next - getMilliSeconds())/1000.0;
        if (timeOut < 0.0)
          timeOut = 0.0;
      }

      ::ReceiveNextEvent(0, nullptr, timeOut, false, &event);
    }

    if (timerMgr->fireTimers(getMilliSeconds())) {
      anyProcessed = true;
    }
#endif
  do {
    {
      ghost_objc::AutoreleasePool pool;
      event = msg<id>(NSApp,
                      GHOST_SEL(nextEventMatchingMask:untilDate:inMode:dequeue:),
                      kNSEventMaskAny,
                      msg<id>(GHOST_CLS(NSDate), GHOST_SEL(distantPast)),
                      NSDefaultRunLoopMode,
                      (signed char)1);
      if (event == nullptr) {
        break;
      }

      anyProcessed = true;

      /* Send event to NSApp to ensure Mac wide events are handled,
       * this will send events to BlenderWindow which will call back
       * to handleKeyEvent, handleMouseEvent and handleTabletEvent. */

      /* There is on special exception for Control+(Shift)+Tab.
       * We do not get keyDown events delivered to the view because they are
       * special hotkeys to switch between views, so override directly */

      if (ev_type(event) == kNSEventTypeKeyDown && ev_keyCode(event) == kVK_Tab &&
          (ev_modifierFlags(event) & kNSEventModifierFlagControl))
      {
        handleKeyEvent(event);
      }
      else {
        /* For some reason NSApp is swallowing the key up events when modifier
         * key is pressed, even if there seems to be no apparent reason to do
         * so, as a workaround we always handle these up events. */
        if (ev_type(event) == kNSEventTypeKeyUp &&
            (ev_modifierFlags(event) & (kNSEventModifierFlagCommand | kNSEventModifierFlagOption)))
        {
          handleKeyEvent(event);
        }

        msg<void>(NSApp, GHOST_SEL(sendEvent:), event);
      }
    }
  } while (event != nullptr);
#if 0
  } while (waitForEvent && !anyProcessed); /* Needed only for timer implementation. */
#endif

  if (m_needDelayedApplicationBecomeActiveEventProcessing) {
    handleApplicationBecomeActiveEvent();
  }

  if (m_outsideLoopEventProcessed) {
    m_outsideLoopEventProcessed = false;
    return true;
  }

  m_ignoreWindowSizedMessages = false;

  return anyProcessed;
}

/* NOTE: called from #NSApplication delegate. */
GHOST_TSuccess GHOST_SystemCocoa::handleApplicationBecomeActiveEvent()
{
  {
    ghost_objc::AutoreleasePool pool;
    for (GHOST_IWindow *iwindow : m_windowManager->getWindows()) {
      GHOST_WindowCocoa *window = (GHOST_WindowCocoa *)iwindow;
      if (window->isDialog()) {
        msg<void>(reinterpret_cast<id>(window->getViewWindow()),
                  GHOST_SEL(makeKeyAndOrderFront:),
                  (id) nullptr);
      }
    }

    /* Update the modifiers key mask, as its status may have changed when the application
     * was not active (that is when update events are sent to another application). */
    GHOST_IWindow *window = m_windowManager->getActiveWindow();

    if (!window) {
      m_needDelayedApplicationBecomeActiveEventProcessing = true;
      return GHOST_kFailure;
    }

    m_needDelayedApplicationBecomeActiveEventProcessing = false;

    const unsigned int modifiers = msg<NSUInteger_>(
        msg<id>(msg<id>(GHOST_CLS(NSApplication), GHOST_SEL(sharedApplication)),
                GHOST_SEL(currentEvent)),
        GHOST_SEL(modifierFlags));

    if ((modifiers & kNSEventModifierFlagShift) != (m_modifierMask & kNSEventModifierFlagShift)) {
      pushEvent(new GHOST_EventKey(getMilliSeconds(),
                                   (modifiers & kNSEventModifierFlagShift) ? GHOST_kEventKeyDown :
                                                                            GHOST_kEventKeyUp,
                                   window,
                                   GHOST_kKeyLeftShift,
                                   false));
    }
    if ((modifiers & kNSEventModifierFlagControl) != (m_modifierMask & kNSEventModifierFlagControl))
    {
      pushEvent(new GHOST_EventKey(getMilliSeconds(),
                                   (modifiers & kNSEventModifierFlagControl) ? GHOST_kEventKeyDown :
                                                                              GHOST_kEventKeyUp,
                                   window,
                                   GHOST_kKeyLeftControl,
                                   false));
    }
    if ((modifiers & kNSEventModifierFlagOption) != (m_modifierMask & kNSEventModifierFlagOption)) {
      pushEvent(new GHOST_EventKey(getMilliSeconds(),
                                   (modifiers & kNSEventModifierFlagOption) ? GHOST_kEventKeyDown :
                                                                             GHOST_kEventKeyUp,
                                   window,
                                   GHOST_kKeyLeftAlt,
                                   false));
    }
    if ((modifiers & kNSEventModifierFlagCommand) != (m_modifierMask & kNSEventModifierFlagCommand))
    {
      pushEvent(new GHOST_EventKey(getMilliSeconds(),
                                   (modifiers & kNSEventModifierFlagCommand) ? GHOST_kEventKeyDown :
                                                                              GHOST_kEventKeyUp,
                                   window,
                                   GHOST_kKeyLeftOS,
                                   false));
    }

    m_modifierMask = modifiers;

    m_outsideLoopEventProcessed = true;
  }
  return GHOST_kSuccess;
}

bool GHOST_SystemCocoa::hasDialogWindow()
{
  for (GHOST_IWindow *iwindow : m_windowManager->getWindows()) {
    GHOST_WindowCocoa *window = (GHOST_WindowCocoa *)iwindow;
    if (window->isDialog()) {
      return true;
    }
  }
  return false;
}

void GHOST_SystemCocoa::notifyExternalEventProcessed()
{
  m_outsideLoopEventProcessed = true;
}

/* NOTE: called from #NSWindow delegate. */
GHOST_TSuccess GHOST_SystemCocoa::handleWindowEvent(GHOST_TEventType eventType,
                                                    GHOST_WindowCocoa *window)
{
  if (!validWindow(window)) {
    return GHOST_kFailure;
  }
  switch (eventType) {
    case GHOST_kEventWindowClose:
      pushEvent(new GHOST_Event(getMilliSeconds(), GHOST_kEventWindowClose, window));
      break;
    case GHOST_kEventWindowActivate:
      m_windowManager->setActiveWindow(window);
      window->loadCursor(window->getCursorVisibility(), window->getCursorShape());
      pushEvent(new GHOST_Event(getMilliSeconds(), GHOST_kEventWindowActivate, window));
      break;
    case GHOST_kEventWindowDeactivate:
      m_windowManager->setWindowInactive(window);
      pushEvent(new GHOST_Event(getMilliSeconds(), GHOST_kEventWindowDeactivate, window));
      break;
    case GHOST_kEventWindowUpdate:
      if (m_nativePixel) {
        window->setNativePixelSize();
        pushEvent(new GHOST_Event(getMilliSeconds(), GHOST_kEventNativeResolutionChange, window));
      }
      pushEvent(new GHOST_Event(getMilliSeconds(), GHOST_kEventWindowUpdate, window));
      break;
    case GHOST_kEventWindowMove:
      pushEvent(new GHOST_Event(getMilliSeconds(), GHOST_kEventWindowMove, window));
      break;
    case GHOST_kEventWindowSize:
      if (!m_ignoreWindowSizedMessages) {
        /* Enforce only one resize message per event loop
         * (coalescing all the live resize messages). */
        window->updateDrawingContext();
        pushEvent(new GHOST_Event(getMilliSeconds(), GHOST_kEventWindowSize, window));
        /* Mouse up event is trapped by the resizing event loop,
         * so send it anyway to the window manager. */
        pushEvent(new GHOST_EventButton(getMilliSeconds(),
                                        GHOST_kEventButtonUp,
                                        window,
                                        GHOST_kButtonMaskLeft,
                                        GHOST_TABLET_DATA_NONE));
        // m_ignoreWindowSizedMessages = true;
      }
      break;
    case GHOST_kEventNativeResolutionChange:

      if (m_nativePixel) {
        window->setNativePixelSize();
        pushEvent(new GHOST_Event(getMilliSeconds(), GHOST_kEventNativeResolutionChange, window));
      }

    default:
      return GHOST_kFailure;
      break;
  }

  m_outsideLoopEventProcessed = true;
  return GHOST_kSuccess;
}

/**
 * Get the true pixel size of an NSImage object.
 * \param image: NSImage to obtain the size of.
 * \return Contained image size in pixels.
 */
static CGSize getNSImagePixelSize(id image)
{
  /* Assuming the NSImage instance only contains one single image. */
  ghost_objc::AutoreleasePool pool;
  id imageRepresentation = msg<id>(msg<id>(image, GHOST_SEL(representations)),
                                   GHOST_SEL(firstObject));
  return CGSizeMake(msg<NSInteger_>(imageRepresentation, GHOST_SEL(pixelsWide)),
                    msg<NSInteger_>(imageRepresentation, GHOST_SEL(pixelsHigh)));
}

/**
 * Convert an NSImage to an ImBuf.
 * \param image: NSImage to convert.
 * \return Pointer to the resulting allocated ImBuf. Caller must free.
 */
static ImBuf *NSImageToImBuf(id image)
{
  const CGSize imageSize = getNSImagePixelSize(image);
  ImBuf *ibuf = IMB_allocImBuf(imageSize.width, imageSize.height, 32, IB_byte_data);

  if (!ibuf) {
    return nullptr;
  }

  {
    ghost_objc::AutoreleasePool pool;
    /* El original usaba enumeracion rapida; sobre un NSArray el recorrido por indice es
     * exactamente equivalente. */
    id bitmapImage = nullptr;
    id representations = msg<id>(image, GHOST_SEL(representations));
    const NSUInteger_ n_reps = count_of(representations);
    for (NSUInteger_ i = 0; i < n_reps; i++) {
      id representation = object_at(representations, i);
      if (msg<signed char>(
              representation, GHOST_SEL(isKindOfClass:), GHOST_CLS(NSBitmapImageRep)))
      {
        bitmapImage = representation;
        break;
      }
    }

    if (bitmapImage == nullptr ||
        msg<NSInteger_>(bitmapImage, GHOST_SEL(bitsPerPixel)) != 32 ||
        msg<signed char>(bitmapImage, GHOST_SEL(isPlanar)) ||
        msg<NSUInteger_>(bitmapImage, GHOST_SEL(bitmapFormat)) &
            (kNSBitmapFormatAlphaFirst | kNSBitmapFormatFloatingPointSamples))
    {
      return nullptr;
    }

    uint8_t *ibuf_data = ibuf->byte_buffer.data;
    uint8_t *bmp_data = msg<uint8_t *>(bitmapImage, GHOST_SEL(bitmapData));

    /* Vertical Flip. */
    for (int y = 0; y < imageSize.height; y++) {
      const int row_byte_count = 4 * imageSize.width;
      const int ibuf_off = (imageSize.height - y - 1) * row_byte_count;
      const int bmp_off = y * row_byte_count;
      memcpy(ibuf_data + ibuf_off, bmp_data + bmp_off, row_byte_count);
    }
  }

  return ibuf;
}

/* NOTE: called from #NSWindow subclass. */
GHOST_TSuccess GHOST_SystemCocoa::handleDraggingEvent(GHOST_TEventType eventType,
                                                      GHOST_TDragnDropTypes draggedObjectType,
                                                      GHOST_WindowCocoa *window,
                                                      int mouseX,
                                                      int mouseY,
                                                      void *data)
{
  if (!validWindow(window)) {
    return GHOST_kFailure;
  }
  switch (eventType) {
    case GHOST_kEventDraggingEntered:
    case GHOST_kEventDraggingUpdated:
    case GHOST_kEventDraggingExited:
      window->clientToScreenIntern(mouseX, mouseY, mouseX, mouseY);
      pushEvent(new GHOST_EventDragnDrop(
          getMilliSeconds(), eventType, draggedObjectType, window, mouseX, mouseY, nullptr));
      break;

    case GHOST_kEventDraggingDropDone: {
      if (!data) {
        return GHOST_kFailure;
      }

      GHOST_TDragnDropDataPtr eventData;
      {
        ghost_objc::AutoreleasePool pool;
        switch (draggedObjectType) {
          case GHOST_kDragnDropTypeFilenames: {
            id droppedArray = (id)data;

            GHOST_TStringArray *strArray = (GHOST_TStringArray *)malloc(
                sizeof(GHOST_TStringArray));
            if (!strArray) {
              return GHOST_kFailure;
            }

            strArray->count = count_of(droppedArray);
            if (strArray->count == 0) {
              free(strArray);
              return GHOST_kFailure;
            }

            strArray->strings = (uint8_t **)malloc(strArray->count * sizeof(uint8_t *));

            for (int i = 0; i < strArray->count; i++) {
              id droppedStr = object_at(droppedArray, i);
              const size_t pastedTextSize = msg<NSUInteger_>(
                  droppedStr, GHOST_SEL(lengthOfBytesUsingEncoding:), kNSUTF8StringEncoding);
              uint8_t *temp_buff = (uint8_t *)malloc(pastedTextSize + 1);

              if (!temp_buff) {
                strArray->count = i;
                break;
              }

              memcpy(temp_buff,
                     msg<const char *>(
                         droppedStr, GHOST_SEL(cStringUsingEncoding:), kNSUTF8StringEncoding),
                     pastedTextSize);
              temp_buff[pastedTextSize] = '\0';

              strArray->strings[i] = temp_buff;
            }

            eventData = static_cast<GHOST_TDragnDropDataPtr>(strArray);
            break;
          }
          case GHOST_kDragnDropTypeString: {
            id droppedStr = (id)data;
            const size_t pastedTextSize = msg<NSUInteger_>(
                droppedStr, GHOST_SEL(lengthOfBytesUsingEncoding:), kNSUTF8StringEncoding);
            uint8_t *temp_buff = (uint8_t *)malloc(pastedTextSize + 1);

            if (temp_buff == nullptr) {
              return GHOST_kFailure;
            }

            memcpy(temp_buff,
                   msg<const char *>(
                       droppedStr, GHOST_SEL(cStringUsingEncoding:), kNSUTF8StringEncoding),
                   pastedTextSize);
            temp_buff[pastedTextSize] = '\0';

            eventData = static_cast<GHOST_TDragnDropDataPtr>(temp_buff);
            break;
          }
          case GHOST_kDragnDropTypeBitmap: {
            id droppedImg = static_cast<id>(data);
            ImBuf *ibuf = NSImageToImBuf(droppedImg);

            eventData = static_cast<GHOST_TDragnDropDataPtr>(ibuf);

            ghost_objc::release(droppedImg);
            break;
          }
          default:
            return GHOST_kFailure;
            break;
        }
      }

      window->clientToScreenIntern(mouseX, mouseY, mouseX, mouseY);
      pushEvent(new GHOST_EventDragnDrop(
          getMilliSeconds(), eventType, draggedObjectType, window, mouseX, mouseY, eventData));

      break;
    }
    default:
      return GHOST_kFailure;
  }
  m_outsideLoopEventProcessed = true;
  return GHOST_kSuccess;
}

void GHOST_SystemCocoa::handleQuitRequest()
{
  GHOST_Window *window = (GHOST_Window *)m_windowManager->getActiveWindow();

  /* Discard quit event if we are in cursor grab sequence. */
  if (window && window->getCursorGrabModeIsWarp()) {
    return;
  }

  /* Push the event to Blender so it can open a dialog if needed. */
  pushEvent(new GHOST_Event(getMilliSeconds(), GHOST_kEventQuitRequest, window));
  m_outsideLoopEventProcessed = true;
}

bool GHOST_SystemCocoa::handleOpenDocumentRequest(void *filepathStr)
{
  id filepath = (id)filepathStr;

  /* Check for blender opened windows and make the front-most key.
   * In case blender is minimized, opened on another desktop space,
   * or in full-screen mode. */
  {
    ghost_objc::AutoreleasePool pool;
    id windowsList = msg<id>(NSApp, GHOST_SEL(orderedWindows));
    if (count_of(windowsList)) {
      msg<void>(object_at(windowsList, 0), GHOST_SEL(makeKeyAndOrderFront:), (id) nullptr);
    }

    GHOST_Window *window = m_windowManager->getWindows().empty() ?
                               nullptr :
                               (GHOST_Window *)m_windowManager->getWindows().front();

    if (!window) {
      return false;
    }

    /* Discard event if we are in cursor grab sequence,
     * it'll lead to "stuck cursor" situation if the alert panel is raised. */
    if (window && window->getCursorGrabModeIsWarp()) {
      return false;
    }

    const size_t filenameTextSize = msg<NSUInteger_>(
        filepath, GHOST_SEL(lengthOfBytesUsingEncoding:), kNSUTF8StringEncoding);
    char *temp_buff = (char *)malloc(filenameTextSize + 1);

    if (temp_buff == nullptr) {
      return GHOST_kFailure;
    }

    memcpy(temp_buff,
           msg<const char *>(filepath, GHOST_SEL(cStringUsingEncoding:), kNSUTF8StringEncoding),
           filenameTextSize);
    temp_buff[filenameTextSize] = '\0';

    pushEvent(new GHOST_EventString(getMilliSeconds(),
                                    GHOST_kEventOpenMainFile,
                                    window,
                                    static_cast<GHOST_TEventDataPtr>(temp_buff)));
  }
  return true;
}

GHOST_TSuccess GHOST_SystemCocoa::handleTabletEvent(void *eventPtr, short eventType)
{
  id event = (id)eventPtr;

  GHOST_IWindow *window = m_windowManager->getWindowAssociatedWithOSWindow(
      (const void *)ev_window(event));
  if (!window) {
    // printf("\nW failure for event 0x%x",ev_type(event));
    return GHOST_kFailure;
  }

  GHOST_TabletData &ct = ((GHOST_WindowCocoa *)window)->GetCocoaTabletData();

  switch (eventType) {
    case kNSEventTypeTabletPoint:
      /* workaround 2 corner-cases:
       * 1. if ev_isEnteringProximity(event) was not triggered since program-start.
       * 2. device is not sending ev_pointingDeviceType(event), due no eraser. */
      if (ct.Active == GHOST_kTabletModeNone) {
        ct.Active = GHOST_kTabletModeStylus;
      }

      ct.Pressure = ev_pressure(event);
      /* Range: -1 (left) to 1 (right). */
      ct.Xtilt = ev_tilt(event).x;
      /* On macOS, the y tilt behavior is inverted from what we expect: negative
       * meaning a tilt toward the user, positive meaning away from the user.
       * Convert to what Blender expects: -1.0 (away from user) to +1.0 (toward user). */
      ct.Ytilt = -ev_tilt(event).y;
      break;

    case kNSEventTypeTabletProximity:
      /* Reset tablet data when device enters proximity or leaves. */
      ct = GHOST_TABLET_DATA_NONE;
      if (ev_isEnteringProximity(event)) {
        /* Pointer is entering tablet area proximity. */
        switch (ev_pointingDeviceType(event)) {
          case kNSPointingDeviceTypePen:
            ct.Active = GHOST_kTabletModeStylus;
            break;
          case kNSPointingDeviceTypeEraser:
            ct.Active = GHOST_kTabletModeEraser;
            break;
          case kNSPointingDeviceTypeCursor:
          case kNSPointingDeviceTypeUnknown:
          default:
            break;
        }
      }
      break;

    default:
      GHOST_ASSERT(FALSE, "GHOST_SystemCocoa::handleTabletEvent : unknown event received");
      return GHOST_kFailure;
      break;
  }
  return GHOST_kSuccess;
}

bool GHOST_SystemCocoa::handleTabletEvent(void *eventPtr)
{
  id event = (id)eventPtr;

  switch (ev_subtype(event)) {
    case kNSEventSubtypeTabletPoint:
      handleTabletEvent(eventPtr, kNSEventTypeTabletPoint);
      return true;
    case kNSEventSubtypeTabletProximity:
      handleTabletEvent(eventPtr, kNSEventTypeTabletProximity);
      return true;
    default:
      /* No tablet event included: do nothing. */
      return false;
  }
}

GHOST_TSuccess GHOST_SystemCocoa::handleMouseEvent(void *eventPtr)
{
  id event = (id)eventPtr;

  /* ev_window(event) returns other windows if mouse-over, that's OSX input standard
   * however, if mouse exits window(s), the windows become inactive, until you click.
   * We then fall back to the active window from ghost. */
  GHOST_WindowCocoa *window = (GHOST_WindowCocoa *)m_windowManager
                                  ->getWindowAssociatedWithOSWindow((const void *)ev_window(event));
  if (!window) {
    window = (GHOST_WindowCocoa *)m_windowManager->getActiveWindow();
    if (!window) {
      // printf("\nW failure for event 0x%x", ev_type(event));
      return GHOST_kFailure;
    }
  }

  switch (ev_type(event)) {
    case kNSEventTypeLeftMouseDown:
      handleTabletEvent(event); /* Update window tablet state to be included in event. */
      pushEvent(new GHOST_EventButton(ev_timestamp(event) * 1000,
                                      GHOST_kEventButtonDown,
                                      window,
                                      GHOST_kButtonMaskLeft,
                                      window->GetCocoaTabletData()));
      break;
    case kNSEventTypeRightMouseDown:
      handleTabletEvent(event); /* Update window tablet state to be included in event. */
      pushEvent(new GHOST_EventButton(ev_timestamp(event) * 1000,
                                      GHOST_kEventButtonDown,
                                      window,
                                      GHOST_kButtonMaskRight,
                                      window->GetCocoaTabletData()));
      break;
    case kNSEventTypeOtherMouseDown:
      handleTabletEvent(event); /* Handle tablet events combined with mouse events. */
      pushEvent(new GHOST_EventButton(ev_timestamp(event) * 1000,
                                      GHOST_kEventButtonDown,
                                      window,
                                      convertButton(ev_buttonNumber(event)),
                                      window->GetCocoaTabletData()));
      break;
    case kNSEventTypeLeftMouseUp:
      handleTabletEvent(event); /* Update window tablet state to be included in event. */
      pushEvent(new GHOST_EventButton(ev_timestamp(event) * 1000,
                                      GHOST_kEventButtonUp,
                                      window,
                                      GHOST_kButtonMaskLeft,
                                      window->GetCocoaTabletData()));
      break;
    case kNSEventTypeRightMouseUp:
      handleTabletEvent(event); /* Update window tablet state to be included in event. */
      pushEvent(new GHOST_EventButton(ev_timestamp(event) * 1000,
                                      GHOST_kEventButtonUp,
                                      window,
                                      GHOST_kButtonMaskRight,
                                      window->GetCocoaTabletData()));
      break;
    case kNSEventTypeOtherMouseUp:
      handleTabletEvent(event); /* Update window tablet state to be included in event. */
      pushEvent(new GHOST_EventButton(ev_timestamp(event) * 1000,
                                      GHOST_kEventButtonUp,
                                      window,
                                      convertButton(ev_buttonNumber(event)),
                                      window->GetCocoaTabletData()));
      break;
    case kNSEventTypeLeftMouseDragged:
    case kNSEventTypeRightMouseDragged:
    case kNSEventTypeOtherMouseDragged:
      handleTabletEvent(event); /* Update window tablet state to be included in event. */

    case kNSEventTypeMouseMoved: {
      GHOST_TGrabCursorMode grab_mode = window->getCursorGrabMode();

      /* TODO: CHECK IF THIS IS A TABLET EVENT */
      bool is_tablet = false;

      if (is_tablet && window->getCursorGrabModeIsWarp()) {
        grab_mode = GHOST_kGrabDisable;
      }

      switch (grab_mode) {
        case GHOST_kGrabHide: {
          /* Cursor hidden grab operation : no cursor move */
          int32_t x_warp, y_warp, x_accum, y_accum, x, y;

          window->getCursorGrabInitPos(x_warp, y_warp);
          window->screenToClientIntern(x_warp, y_warp, x_warp, y_warp);

          /* Strange Apple implementation (inverted coordinates for the deltaY)... */
          window->getCursorGrabAccum(x_accum, y_accum);
          x_accum += ev_deltaX(event);
          y_accum += -ev_deltaY(event);
          window->setCursorGrabAccum(x_accum, y_accum);

          window->clientToScreenIntern(x_warp + x_accum, y_warp + y_accum, x, y);
          pushEvent(new GHOST_EventCursor(ev_timestamp(event) * 1000,
                                          GHOST_kEventCursorMove,
                                          window,
                                          x,
                                          y,
                                          window->GetCocoaTabletData()));
          break;
        }
        case GHOST_kGrabWrap: {
          /* Wrap cursor at area/window boundaries. */
          const NSTimeInterval timestamp = ev_timestamp(event);
          if (timestamp < m_last_warp_timestamp) {
            /* After warping we can still receive older unwrapped mouse events,
             * ignore those. */
            break;
          }

          GHOST_Rect bounds, windowBounds, correctedBounds;

          /* fall back to window bounds */
          if (window->getCursorGrabBounds(bounds) == GHOST_kFailure) {
            window->getClientBounds(bounds);
          }

          /* Switch back to Cocoa coordinates orientation
           * (y=0 at bottom, the same as blender internal BTW!), and to client coordinates. */
          window->getClientBounds(windowBounds);
          window->screenToClient(bounds.m_l, bounds.m_b, correctedBounds.m_l, correctedBounds.m_t);
          window->screenToClient(bounds.m_r, bounds.m_t, correctedBounds.m_r, correctedBounds.m_b);
          correctedBounds.m_b = (windowBounds.m_b - windowBounds.m_t) - correctedBounds.m_b;
          correctedBounds.m_t = (windowBounds.m_b - windowBounds.m_t) - correctedBounds.m_t;

          /* Get accumulation from previous mouse warps. */
          int32_t x_accum, y_accum;
          window->getCursorGrabAccum(x_accum, y_accum);

          const NSPoint mousePos = ev_locationInWindow(event);
          /* Casting. */
          const int32_t x_mouse = mousePos.x;
          const int32_t y_mouse = mousePos.y;

          /* Warp mouse cursor if needed. */
          int32_t warped_x_mouse = x_mouse;
          int32_t warped_y_mouse = y_mouse;
          correctedBounds.wrapPoint(
              warped_x_mouse, warped_y_mouse, 4, window->getCursorGrabAxis());

          /* Set new cursor position. */
          if (x_mouse != warped_x_mouse || y_mouse != warped_y_mouse) {
            int32_t warped_x, warped_y;
            window->clientToScreenIntern(warped_x_mouse, warped_y_mouse, warped_x, warped_y);
            setMouseCursorPosition(warped_x, warped_y); /* wrap */
            window->setCursorGrabAccum(x_accum + (x_mouse - warped_x_mouse),
                                       y_accum + (y_mouse - warped_y_mouse));

            /* This is the current time that matches NSEvent timestamp. */
            m_last_warp_timestamp = msg<double>(
                msg<id>(GHOST_CLS(NSProcessInfo), GHOST_SEL(processInfo)),
                GHOST_SEL(systemUptime));
          }

          /* Generate event. */
          int32_t x, y;
          window->clientToScreenIntern(x_mouse + x_accum, y_mouse + y_accum, x, y);
          pushEvent(new GHOST_EventCursor(ev_timestamp(event) * 1000,
                                          GHOST_kEventCursorMove,
                                          window,
                                          x,
                                          y,
                                          window->GetCocoaTabletData()));
          break;
        }
        default: {
          /* Normal cursor operation: send mouse position in window. */
          const NSPoint mousePos = ev_locationInWindow(event);
          int32_t x, y;

          window->clientToScreenIntern(mousePos.x, mousePos.y, x, y);
          pushEvent(new GHOST_EventCursor(ev_timestamp(event) * 1000,
                                          GHOST_kEventCursorMove,
                                          window,
                                          x,
                                          y,
                                          window->GetCocoaTabletData()));
          break;
        }
      }
      break;
    }
    case kNSEventTypeScrollWheel: {
      const NSUInteger_ momentumPhase = ev_momentumPhase(event);
      const NSUInteger_ phase = ev_phase(event);

      /* when pressing a key while momentum scrolling continues after
       * lifting fingers off the trackpad, the action can unexpectedly
       * change from e.g. scrolling to zooming. this works around the
       * issue by ignoring momentum scroll after a key press */
      if (momentumPhase) {
        if (m_ignoreMomentumScroll) {
          break;
        }
      }
      else {
        m_ignoreMomentumScroll = false;
      }

      /* we assume phases are only set for gestures from trackpad or magic
       * mouse events. note that using tablet at the same time may not work
       * since this is a static variable */
      if (phase == kNSEventPhaseBegan && m_multitouchGestures) {
        m_multiTouchScroll = true;
      }
      else if (phase == kNSEventPhaseEnded) {
        m_multiTouchScroll = false;
      }

      /* Standard scroll-wheel case, if no swiping happened,
       * and no momentum (kinetic scroll) works. */
      if (!m_multiTouchScroll && momentumPhase == kNSEventPhaseNone) {
        if (ev_deltaX(event) != 0.0) {
          const int32_t delta = ev_deltaX(event) > 0.0 ? 1 : -1;
          pushEvent(new GHOST_EventWheel(
              ev_timestamp(event) * 1000, window, GHOST_kEventWheelAxisHorizontal, delta));
        }
        if (ev_deltaY(event) != 0.0) {
          const int32_t delta = ev_deltaY(event) > 0.0 ? 1 : -1;
          pushEvent(new GHOST_EventWheel(
              ev_timestamp(event) * 1000, window, GHOST_kEventWheelAxisVertical, delta));
        }
      }
      else {
        const NSPoint mousePos = ev_locationInWindow(event);

        /* with 10.7 nice scrolling deltas are supported */
        double dx = ev_scrollingDeltaX(event);
        double dy = ev_scrollingDeltaY(event);

        /* However, WACOM tablet (intuos5) needs old deltas,
         * it then has momentum and phase at zero. */
        if (phase == kNSEventPhaseNone && momentumPhase == kNSEventPhaseNone) {
          dx = ev_deltaX(event);
          dy = ev_deltaY(event);
        }

        int32_t x, y;
        window->clientToScreenIntern(mousePos.x, mousePos.y, x, y);

        BlenderWindow *view_window = (BlenderWindow *)window->getOSWindow();

        {
          ghost_objc::AutoreleasePool pool;
          const CGPoint delta = msg<CGPoint>(
              msg<id>(reinterpret_cast<id>(view_window), GHOST_SEL(contentView)),
              GHOST_SEL(convertPointToBacking:),
              CGPointMake(dx, dy));
          pushEvent(new GHOST_EventTrackpad(ev_timestamp(event) * 1000,
                                            window,
                                            GHOST_kTrackpadEventScroll,
                                            x,
                                            y,
                                            delta.x,
                                            delta.y,
                                            ev_isDirectionInvertedFromDevice(event)));
        }
      }
      break;
    }
    case kNSEventTypeMagnify: {
      const NSPoint mousePos = ev_locationInWindow(event);
      int32_t x, y;
      window->clientToScreenIntern(mousePos.x, mousePos.y, x, y);
      pushEvent(new GHOST_EventTrackpad(ev_timestamp(event) * 1000,
                                        window,
                                        GHOST_kTrackpadEventMagnify,
                                        x,
                                        y,
                                        ev_magnification(event) * 125.0 + 0.1,
                                        0,
                                        false));
      break;
    }
    case kNSEventTypeSmartMagnify: {
      const NSPoint mousePos = ev_locationInWindow(event);
      int32_t x, y;
      window->clientToScreenIntern(mousePos.x, mousePos.y, x, y);
      pushEvent(new GHOST_EventTrackpad(
          ev_timestamp(event) * 1000, window, GHOST_kTrackpadEventSmartMagnify, x, y, 0, 0, false));
      break;
    }
    case kNSEventTypeRotate: {
      const NSPoint mousePos = ev_locationInWindow(event);
      int32_t x, y;
      window->clientToScreenIntern(mousePos.x, mousePos.y, x, y);
      pushEvent(new GHOST_EventTrackpad(ev_timestamp(event) * 1000,
                                        window,
                                        GHOST_kTrackpadEventRotate,
                                        x,
                                        y,
                                        ev_rotation(event) * -5.0,
                                        0,
                                        false));
    }
    default:
      return GHOST_kFailure;
      break;
  }
  return GHOST_kSuccess;
}

GHOST_TSuccess GHOST_SystemCocoa::handleKeyEvent(void *eventPtr)
{
  id event = (id)eventPtr;
  GHOST_IWindow *window = m_windowManager->getWindowAssociatedWithOSWindow(
      (const void *)ev_window(event));

  if (!window) {
    // printf("\nW failure for event 0x%x",ev_type(event));
    return GHOST_kFailure;
  }

  switch (ev_type(event)) {
    case kNSEventTypeKeyDown:
    case kNSEventTypeKeyUp: {
      /* Returns an empty string for dead keys. */
      GHOST_TKey keyCode;
      char utf8_buf[6] = {'\0'};

      {
        ghost_objc::AutoreleasePool pool;
        id charsIgnoringModifiers = ev_charactersIgnoringModifiers(event);
        if (msg<NSUInteger_>(charsIgnoringModifiers, GHOST_SEL(length)) > 0) {
          keyCode = convertKey(
              ev_keyCode(event),
              msg<unsigned short>(
                  charsIgnoringModifiers, GHOST_SEL(characterAtIndex:), (NSUInteger_)0));
        }
        else {
          keyCode = convertKey(ev_keyCode(event), 0);
        }

        id characters = ev_characters(event);
        if (msg<NSUInteger_>(characters, GHOST_SEL(length)) > 0) {
          id convertedCharacters = msg<id>(
              characters, GHOST_SEL(dataUsingEncoding:), kNSUTF8StringEncoding);

          const NSUInteger_ n_bytes = msg<NSUInteger_>(convertedCharacters, GHOST_SEL(length));
          const char *raw_bytes = msg<const char *>(convertedCharacters, GHOST_SEL(bytes));
          for (NSUInteger_ x = 0; x < n_bytes; x++) {
            utf8_buf[x] = raw_bytes[x];
          }
        }
      }

      /* Arrow keys should not have UTF8. */
      if ((keyCode >= GHOST_kKeyLeftArrow) && (keyCode <= GHOST_kKeyDownArrow)) {
        utf8_buf[0] = '\0';
      }

      /* F-keys should not have UTF8. */
      if ((keyCode >= GHOST_kKeyF1) && (keyCode <= GHOST_kKeyF20)) {
        utf8_buf[0] = '\0';
      }

      /* no text with command key pressed */
      if (m_modifierMask & kNSEventModifierFlagCommand) {
        utf8_buf[0] = '\0';
      }

      if ((keyCode == GHOST_kKeyQ) && (m_modifierMask & kNSEventModifierFlagCommand)) {
        break; /* Command-Q is directly handled by Cocoa. */
      }

      if (ev_type(event) == kNSEventTypeKeyDown) {
        pushEvent(new GHOST_EventKey(ev_timestamp(event) * 1000,
                                     GHOST_kEventKeyDown,
                                     window,
                                     keyCode,
                                     ev_isARepeat(event),
                                     utf8_buf));
#if 0
        printf("Key down rawCode=0x%x charsIgnoringModifiers=%c keyCode=%u utf8=%s\n",
               ev_keyCode(event),
               msg<NSUInteger_>(charsIgnoringModifiers, GHOST_SEL(length)) > 0 ?
                   msg<unsigned short>(
                       charsIgnoringModifiers, GHOST_SEL(characterAtIndex:), (NSUInteger_)0) :
                   ' ',
               keyCode,
               utf8_buf);
#endif
      }
      else {
        pushEvent(new GHOST_EventKey(
            ev_timestamp(event) * 1000, GHOST_kEventKeyUp, window, keyCode, false, nullptr));
#if 0
        printf("Key up rawCode=0x%x charsIgnoringModifiers=%c keyCode=%u utf8=%s\n",
               ev_keyCode(event),
               msg<NSUInteger_>(charsIgnoringModifiers, GHOST_SEL(length)) > 0 ?
                   msg<unsigned short>(
                       charsIgnoringModifiers, GHOST_SEL(characterAtIndex:), (NSUInteger_)0) :
                   ' ',
               keyCode,
               utf8_buf);
#endif
      }
      m_ignoreMomentumScroll = true;
      break;
    }
    case kNSEventTypeFlagsChanged: {
      const unsigned int modifiers = ev_modifierFlags(event);

      if ((modifiers & kNSEventModifierFlagShift) != (m_modifierMask & kNSEventModifierFlagShift)) {
        pushEvent(new GHOST_EventKey(ev_timestamp(event) * 1000,
                                     (modifiers & kNSEventModifierFlagShift) ? GHOST_kEventKeyDown :
                                                                              GHOST_kEventKeyUp,
                                     window,
                                     GHOST_kKeyLeftShift,
                                     false));
      }
      if ((modifiers & kNSEventModifierFlagControl) !=
          (m_modifierMask & kNSEventModifierFlagControl))
      {
        pushEvent(new GHOST_EventKey(
            ev_timestamp(event) * 1000,
            (modifiers & kNSEventModifierFlagControl) ? GHOST_kEventKeyDown : GHOST_kEventKeyUp,
            window,
            GHOST_kKeyLeftControl,
            false));
      }
      if ((modifiers & kNSEventModifierFlagOption) != (m_modifierMask & kNSEventModifierFlagOption))
      {
        pushEvent(new GHOST_EventKey(
            ev_timestamp(event) * 1000,
            (modifiers & kNSEventModifierFlagOption) ? GHOST_kEventKeyDown : GHOST_kEventKeyUp,
            window,
            GHOST_kKeyLeftAlt,
            false));
      }
      if ((modifiers & kNSEventModifierFlagCommand) !=
          (m_modifierMask & kNSEventModifierFlagCommand))
      {
        pushEvent(new GHOST_EventKey(
            ev_timestamp(event) * 1000,
            (modifiers & kNSEventModifierFlagCommand) ? GHOST_kEventKeyDown : GHOST_kEventKeyUp,
            window,
            GHOST_kKeyLeftOS,
            false));
      }

      m_modifierMask = modifiers;
      m_ignoreMomentumScroll = true;
      break;
    }

    default:
      return GHOST_kFailure;
      break;
  }
  return GHOST_kSuccess;
}

/* --------------------------------------------------------------------
 * Clipboard get/set.
 */

char *GHOST_SystemCocoa::getClipboard(bool /*selection*/) const
{
  ghost_objc::AutoreleasePool pool;
  id pasteBoard = msg<id>(GHOST_CLS(NSPasteboard), GHOST_SEL(generalPasteboard));
  id textPasted = msg<id>(pasteBoard, GHOST_SEL(stringForType:), NSPasteboardTypeString);

  if (textPasted == nullptr) {
    return nullptr;
  }

  const size_t pastedTextSize = msg<NSUInteger_>(
      textPasted, GHOST_SEL(lengthOfBytesUsingEncoding:), kNSUTF8StringEncoding);

  char *temp_buff = (char *)malloc(pastedTextSize + 1);

  if (temp_buff == nullptr) {
    return nullptr;
  }

  memcpy(temp_buff,
         msg<const char *>(textPasted, GHOST_SEL(cStringUsingEncoding:), kNSUTF8StringEncoding),
         pastedTextSize);
  temp_buff[pastedTextSize] = '\0';

  return temp_buff;
}

void GHOST_SystemCocoa::putClipboard(const char *buffer, bool selection) const
{
  if (selection) {
    return; /* For copying the selection, used on X11. */
  }

  ghost_objc::AutoreleasePool pool;
  id pasteBoard = msg<id>(GHOST_CLS(NSPasteboard), GHOST_SEL(generalPasteboard));
  /* `@[ NSPasteboardTypeString ]` es `[NSArray arrayWithObjects:..., nil]`. */
  msg<void>(pasteBoard,
            GHOST_SEL(declareTypes:owner:),
            ns_array(NSPasteboardTypeString),
            (id) nullptr);

  id textToCopy = msg<id>(
      GHOST_CLS(NSString), GHOST_SEL(stringWithCString:encoding:), buffer, kNSUTF8StringEncoding);
  msg<void>(pasteBoard, GHOST_SEL(setString:forType:), textToCopy, NSPasteboardTypeString);
}

static id NSPasteboardGetImageFile()
{
  id pasteboardImageFile = nullptr;

  {
    ghost_objc::AutoreleasePool pool;
    id pasteboard = msg<id>(GHOST_CLS(NSPasteboard), GHOST_SEL(generalPasteboard));
    /* `@{ k1 : v1, k2 : v2 }` es
     * `[NSDictionary dictionaryWithObjectsAndKeys:v1, k1, v2, k2, nil]`. OJO al orden:
     * el diccionario literal va CLAVE : VALOR y este constructor va VALOR, CLAVE.
     * `@YES` es `[NSNumber numberWithBool:YES]`. */
    id yes_number = msg<id>(GHOST_CLS(NSNumber), GHOST_SEL(numberWithBool:), (signed char)1);
    id image_types = msg<id>(GHOST_CLS(NSImage), GHOST_SEL(imageTypes));
    id pasteboardFilteringOptions = msg<id>(GHOST_CLS(NSDictionary),
                                            GHOST_SEL(dictionaryWithObjectsAndKeys:),
                                            yes_number,
                                            NSPasteboardURLReadingFileURLsOnlyKey,
                                            image_types,
                                            NSPasteboardURLReadingContentsConformToTypesKey,
                                            (id) nullptr);

    id pasteboardMatches = msg<id>(pasteboard,
                                   GHOST_SEL(readObjectsForClasses:options:),
                                   ns_array(GHOST_CLS(NSURL)),
                                   pasteboardFilteringOptions);

    if (!pasteboardMatches || !count_of(pasteboardMatches)) {
      return nullptr;
    }

    pasteboardImageFile = msg<id>(msg<id>(pasteboardMatches, GHOST_SEL(firstObject)),
                                  GHOST_SEL(copy));
  }

  return ghost_objc::autorelease(pasteboardImageFile);
}

GHOST_TSuccess GHOST_SystemCocoa::hasClipboardImage() const
{
  ghost_objc::AutoreleasePool pool;
  id pasteboard = msg<id>(GHOST_CLS(NSPasteboard), GHOST_SEL(generalPasteboard));
  id supportedTypes = ns_array(NSPasteboardTypeFileURL, NSPasteboardTypeTIFF, NSPasteboardTypePNG);

  id availableType = msg<id>(pasteboard, GHOST_SEL(availableTypeFromArray:), supportedTypes);

  if (!availableType) {
    return GHOST_kFailure;
  }

  /* If we got a file, ensure it's an image file. */
  if (msg<id>(pasteboard, GHOST_SEL(availableTypeFromArray:), ns_array(NSPasteboardTypeFileURL)) &&
      NSPasteboardGetImageFile() == nullptr)
  {
    return GHOST_kFailure;
  }

  return GHOST_kSuccess;
}

uint *GHOST_SystemCocoa::getClipboardImage(int *r_width, int *r_height) const
{
  if (!hasClipboardImage()) {
    return nullptr;
  }

  ghost_objc::AutoreleasePool pool;
  id pasteboard = msg<id>(GHOST_CLS(NSPasteboard), GHOST_SEL(generalPasteboard));

  id clipboardImage = nullptr;
  if (id pasteboardImageFile = NSPasteboardGetImageFile(); pasteboardImageFile != nullptr) {
    /* Image file. */
    clipboardImage = ghost_objc::autorelease(msg<id>(msg<id>(GHOST_CLS(NSImage), GHOST_SEL(alloc)),
                                                     GHOST_SEL(initWithContentsOfURL:),
                                                     pasteboardImageFile));
  }
  else {
    /* Raw image data. */
    clipboardImage = ghost_objc::autorelease(msg<id>(msg<id>(GHOST_CLS(NSImage), GHOST_SEL(alloc)),
                                                     GHOST_SEL(initWithPasteboard:),
                                                     pasteboard));
  }

  if (!clipboardImage) {
    return nullptr;
  }

  ImBuf *ibuf = NSImageToImBuf(clipboardImage);
  const CGSize clipboardImageSize = getNSImagePixelSize(clipboardImage);

  if (ibuf) {
    const size_t byteCount = clipboardImageSize.width * clipboardImageSize.height * 4;
    uint *rgba = (uint *)malloc(byteCount);

    if (!rgba) {
      IMB_freeImBuf(ibuf);
      return nullptr;
    }

    memcpy(rgba, ibuf->byte_buffer.data, byteCount);
    IMB_freeImBuf(ibuf);

    *r_width = clipboardImageSize.width;
    *r_height = clipboardImageSize.height;

    return rgba;
  }

  return nullptr;
}

GHOST_TSuccess GHOST_SystemCocoa::putClipboardImage(uint *rgba, int width, int height) const
{
  ghost_objc::AutoreleasePool pool;
  const size_t rowByteCount = width * 4;

  /* Este selector es demasiado largo para partirlo dentro de `GHOST_SEL(...)`: el `#`
   * del preprocesador convertiria el salto de linea en un ESPACIO y registraria un
   * selector que no existe. Va como literal de una sola pieza. */
  static const SEL sel_initBitmapRep = ghost_objc::sel(
      "initWithBitmapDataPlanes:pixelsWide:pixelsHigh:bitsPerSample:samplesPerPixel:hasAlpha:"
      "isPlanar:colorSpaceName:bytesPerRow:bitsPerPixel:");
  id imageRep = msg<id>(msg<id>(GHOST_CLS(NSBitmapImageRep), GHOST_SEL(alloc)),
                        sel_initBitmapRep,
                        (unsigned char **)nullptr,
                        (NSInteger_)width,
                        (NSInteger_)height,
                        (NSInteger_)8,
                        (NSInteger_)4,
                        (signed char)1,
                        (signed char)0,
                        NSDeviceRGBColorSpace,
                        (NSInteger_)rowByteCount,
                        (NSInteger_)32);

  /* Copy the source image data to imageRep, flipping it vertically. */
  uint8_t *srcBuffer = reinterpret_cast<uint8_t *>(rgba);
  uint8_t *dstBuffer = msg<uint8_t *>(imageRep, GHOST_SEL(bitmapData));

  for (int y = 0; y < height; y++) {
    const int dstOff = (height - y - 1) * rowByteCount;
    const int srcOff = y * rowByteCount;
    memcpy(dstBuffer + dstOff, srcBuffer + srcOff, rowByteCount);
  }

  id image = ghost_objc::autorelease(msg<id>(msg<id>(GHOST_CLS(NSImage), GHOST_SEL(alloc)),
                                             GHOST_SEL(initWithSize:),
                                             CGSizeMake(width, height)));
  msg<void>(image, GHOST_SEL(addRepresentation:), imageRep);

  id pasteboard = msg<id>(GHOST_CLS(NSPasteboard), GHOST_SEL(generalPasteboard));
  msg<void>(pasteboard, GHOST_SEL(clearContents));

  const bool pasteSuccess = msg<signed char>(
                                pasteboard, GHOST_SEL(writeObjects:), ns_array(image)) != 0;

  if (!pasteSuccess) {
    return GHOST_kFailure;
  }
  return GHOST_kSuccess;
}

GHOST_TSuccess GHOST_SystemCocoa::showMessageBox(const char *title,
                                                 const char *message,
                                                 const char *help_label,
                                                 const char *continue_label,
                                                 const char *link,
                                                 GHOST_DialogOptions dialog_options) const
{
  ghost_objc::AutoreleasePool pool;
  id alert = ghost_objc::autorelease(ghost_objc::alloc_init("NSAlert"));
  msg<void>(alert,
            GHOST_SEL(setAccessoryView:),
            ghost_objc::autorelease(msg<id>(msg<id>(GHOST_CLS(NSView), GHOST_SEL(alloc)),
                                            GHOST_SEL(initWithFrame:),
                                            CGRectMake(0, 0, 500, 0))));

  /* El original usaba `[NSString stringWithCString:]` a secas, que esta OBSOLETO desde
   * 10.4 y supone la codificacion por defecto del sistema. Se conserva el mismo
   * selector para no cambiar la conducta observable; migrar a
   * `stringWithCString:encoding:` seria una mejora, pero es otra decision. */
  static const SEL sel_stringWithCString = ghost_objc::sel("stringWithCString:");
  id titleString = msg<id>(GHOST_CLS(NSString), sel_stringWithCString, title);
  id messageString = msg<id>(GHOST_CLS(NSString), sel_stringWithCString, message);
  id continueString = msg<id>(GHOST_CLS(NSString), sel_stringWithCString, continue_label);
  id helpString = msg<id>(GHOST_CLS(NSString), sel_stringWithCString, help_label);

  if (dialog_options & GHOST_DialogError) {
    msg<void>(alert, GHOST_SEL(setAlertStyle:), kNSAlertStyleCritical);
  }
  else if (dialog_options & GHOST_DialogWarning) {
    msg<void>(alert, GHOST_SEL(setAlertStyle:), kNSAlertStyleWarning);
  }
  else {
    msg<void>(alert, GHOST_SEL(setAlertStyle:), kNSAlertStyleInformational);
  }

  msg<void>(alert, GHOST_SEL(setMessageText:), titleString);
  msg<void>(alert, GHOST_SEL(setInformativeText:), messageString);

  msg<id>(alert, GHOST_SEL(addButtonWithTitle:), continueString);
  if (link && strlen(link)) {
    msg<id>(alert, GHOST_SEL(addButtonWithTitle:), helpString);
  }

  const NSInteger_ response = msg<NSInteger_>(alert, GHOST_SEL(runModal));
  if (response == kNSAlertSecondButtonReturn) {
    id linkString = msg<id>(GHOST_CLS(NSString), sel_stringWithCString, link);
    msg<void>(msg<id>(GHOST_CLS(NSWorkspace), GHOST_SEL(sharedWorkspace)),
              GHOST_SEL(openURL:),
              msg<id>(GHOST_CLS(NSURL), GHOST_SEL(URLWithString:), linkString));
  }
  return GHOST_kSuccess;
}
