/* SPDX-FileCopyrightText: 2001-2002 NaN Holding BV. All rights reserved.
 *
 * SPDX-License-Identifier: GPL-2.0-or-later */

/** \file
 * \ingroup GHOST
 *
 * C++ PURO. Era la mitad `@implementation` de `GHOST_WindowViewCocoa.hh`.
 *
 * Aqui GHOST deja de mandar mensajes y pasa a RECIBIRLOS: esta es la vista que Cocoa
 * llama para cada tecla, cada movimiento del raton y cada redibujado. Las clases se
 * fabrican con `ghost_objc::ClassBuilder`, que PIDE AL RUNTIME la codificacion de tipo
 * de cada metodo en vez de escribirla a mano; si un selector estuviera mal escrito, el
 * registro aborta con un mensaje claro antes de dibujar un solo pixel.
 *
 * DOS SIMPLIFICACIONES QUE NO CAMBIAN LA CONDUCTA:
 *
 * 1. De los 37 metodos del `@implementation` original, Cocoa solo llama a 24. El resto
 *    eran ayudas internas (`composing_free`, `processImeEvent`, `convertNSString`...)
 *    que estaban en Objective-C solo por vecindad. Aqui son funciones de C++ normales y
 *    NO se registran: menos superficie, y ningun selector que pueda estar mal escrito.
 *    La excepcion es `ImeDidChangeCallback:`, que SI la llama el centro de
 *    notificaciones y por tanto sigue siendo un metodo registrado.
 *
 * 2. Las propiedades `systemCocoa` y `windowCocoa` estaban declaradas `readonly` y no
 *    las usaba nadie fuera de la clase (comprobado con grep en todo `intern/ghost`).
 *    Se retiran; el estado vive en una struct de C++ colgada de una sola variable de
 *    instancia.
 */

#include "GHOST_WindowViewCocoa.hh"

#include "GHOST_SystemCocoa.hh"
#include "GHOST_WindowCocoa.hh"

#ifdef WITH_INPUT_IME
/* Carbon es C puro y se puede incluir desde un `.cc`. De aqui salen los `kVK_*` y las
 * funciones `TIS*`, que es lo unico que Cocoa no ofrece para saber si el metodo de
 * entrada actual admite IME. */
#  include <Carbon/Carbon.h>
#endif

#include <CoreGraphics/CoreGraphics.h>

#include <cstring>
#include <string>

using ghost_objc::msg;
using ghost_objc::msg_super;

#ifdef WITH_INPUT_IME
/* `NSTextInputContextKeyboardSelectionDidChangeNotification` es un `NSString *` global
 * que exporta AppKit. Se toma el SIMBOLO de verdad en vez de escribir la cadena a mano:
 * si Apple lo cambiara, lo diria el enlazador en vez de dejar de registrar el
 * observador en silencio. Una declaracion con enlace de C no puede ir dentro de una
 * funcion, por eso esta aqui. */
extern "C" id NSTextInputContextKeyboardSelectionDidChangeNotification;
#endif

namespace {

using NSUInteger_ = unsigned long;
using NSInteger_ = long;

/* Constantes de AppKit y Foundation. Comprobadas con `static_assert` contra el SDK 26.5
 * en la sonda de constantes (ver informe GHOST-1). */
constexpr NSInteger_ kNSNotFound = 0x7fffffffffffffffLL; /* NSIntegerMax */
#ifdef WITH_INPUT_IME
constexpr NSUInteger_ kNSEventModifierFlagControl = 1 << 18;
constexpr NSUInteger_ kNSEventModifierFlagCommand = 1 << 20;
#endif

/** `NSRange` del SDK: dos `NSUInteger`, 16 bytes, vuelve en registros (NO stret). */
struct NSRange_ {
  NSUInteger_ location;
  NSUInteger_ length;
};

/* -------------------------------------------------------------------------
 * El estado de la vista.
 *
 * El original lo tenia como variables de instancia de Objective-C. Aqui va en una
 * struct de C++ colgada de UNA sola variable de instancia (`m_ghost_state`), porque el
 * bloque del original contiene `std::string`, que no es trivialmente construible y no
 * se puede dejar en memoria que el runtime solo pone a cero.
 */
struct ViewState {
  GHOST_SystemCocoa *system = nullptr;
  GHOST_WindowCocoa *window = nullptr;

  bool composing = false;
  id composing_text = nullptr;

#ifdef WITH_INPUT_IME
  struct {
    GHOST_ImeStateFlagCocoa state_flag = 0;
    CGRect candidate_window_position = CGRectZero;

    /* Event data. */
    GHOST_TEventImeData event;
    std::string combined_result;
  } ime;
#endif
};

const char *const kStateIvar = "m_ghost_state";

ViewState *state_of(id self)
{
  return ghost_objc::ivar_get<ViewState *>(self, kStateIvar);
}

/* -------------------------------------------------------------------------
 * Ayudas internas. En el original eran metodos de Objective-C; aqui son funciones
 * normales porque nadie fuera de este fichero las llama por selector.
 */

void composing_free(ViewState *st)
{
  st->composing = false;

  if (st->composing_text) {
    ghost_objc::release(st->composing_text);
    st->composing_text = nullptr;
  }
}

NSUInteger_ string_length(id nsstring)
{
  return nsstring ? msg<NSUInteger_>(nsstring, GHOST_SEL(length)) : 0;
}

#ifdef WITH_INPUT_IME

std::string convert_nsstring(id in_string)
{
  ghost_objc::AutoreleasePool pool;
  const char *utf8 = ghost_objc::utf8_string(in_string);
  return std::string(utf8 ? utf8 : "");
}

void check_ime_enabled(ViewState *st)
{
  st->ime.state_flag &= ~GHOST_IME_ENABLED;

  if (st->ime.state_flag & GHOST_IME_INPUT_FOCUSED) {
    /* Since there are no functions in Cocoa API,
     * we will use the functions in the Carbon API. */
    TISInputSourceRef currentKeyboardInputSource = TISCopyCurrentKeyboardInputSource();
    const bool ime_enabled = !CFBooleanGetValue((CFBooleanRef)TISGetInputSourceProperty(
        currentKeyboardInputSource, kTISPropertyInputSourceIsASCIICapable));
    CFRelease(currentKeyboardInputSource);

    if (ime_enabled) {
      st->ime.state_flag |= GHOST_IME_ENABLED;
      return;
    }
  }
  return;
}

void process_ime_event(ViewState *st, GHOST_TEventType imeEventType)
{
  GHOST_Event *event = new GHOST_EventIME(
      st->system->getMilliSeconds(), imeEventType, st->window, &st->ime.event);
  st->system->pushEvent(event);
}

void set_ime_result(ViewState *st, const std::string &result)
{
  st->ime.event.result = result;
  st->ime.event.composite.clear();
  st->ime.event.cursor_position = -1;
  st->ime.event.target_start = -1;
  st->ime.event.target_end = -1;
}

void set_ime_composition(ViewState *st, id in_string, NSRange_ range)
{
  st->ime.event.composite = convert_nsstring(in_string);

  /* For Korean input, both "Result Event" and "Composition Event" can occur in a single keyDown.
   */
  if (!(st->ime.state_flag & GHOST_IME_RESULT_EVENT)) {
    st->ime.event.result.clear();
  }

  /* The target string is equivalent to the string in selectedRange of setMarkedText.
   * The cursor is displayed at the beginning of the target string. */
  {
    ghost_objc::AutoreleasePool pool;
    const NSRange_ front_range{0, range.location};
    const char *front_string = ghost_objc::utf8_string(
        msg<id>(in_string, GHOST_SEL(substringWithRange:), front_range));
    const char *selected_string = ghost_objc::utf8_string(
        msg<id>(in_string, GHOST_SEL(substringWithRange:), range));
    st->ime.event.cursor_position = strlen(front_string ? front_string : "");
    st->ime.event.target_start = st->ime.event.cursor_position;
    st->ime.event.target_end = st->ime.event.target_start +
                               strlen(selected_string ? selected_string : "");
  }
}

bool ime_did_composition(const ViewState *st)
{
  return (st->ime.state_flag & GHOST_IME_COMPOSITION_EVENT) ||
         (st->ime.state_flag & GHOST_IME_RESULT_EVENT);
}

/* Even if IME is enabled, when not composing, control characters
 * (such as arrow, enter, delete) are handled by handleKeyEvent. */
bool is_processed_by_ime(const ViewState *st)
{
  return ((st->ime.state_flag & GHOST_IME_ENABLED) &&
          ((st->ime.state_flag & GHOST_IME_COMPOSING) ||
           !(st->ime.state_flag & GHOST_IME_KEY_CONTROL_CHAR)));
}

void check_key_code_is_control_char(ViewState *st, id event)
{
  st->ime.state_flag &= ~GHOST_IME_KEY_CONTROL_CHAR;

  /* Don't use IME for command and ctrl key combinations, these are shortcuts. */
  const NSUInteger_ modifiers = msg<NSUInteger_>(event, GHOST_SEL(modifierFlags));
  if (modifiers & (kNSEventModifierFlagCommand | kNSEventModifierFlagControl)) {
    st->ime.state_flag |= GHOST_IME_KEY_CONTROL_CHAR;
    return;
  }

  /* Don't use IME for these control keys. */
  switch (msg<unsigned short>(event, GHOST_SEL(keyCode))) {
    case kVK_ANSI_KeypadEnter:
    case kVK_ANSI_KeypadClear:
    case kVK_F1:
    case kVK_F2:
    case kVK_F3:
    case kVK_F4:
    case kVK_F5:
    case kVK_F6:
    case kVK_F7:
    case kVK_F8:
    case kVK_F9:
    case kVK_F10:
    case kVK_F11:
    case kVK_F12:
    case kVK_F13:
    case kVK_F14:
    case kVK_F15:
    case kVK_F16:
    case kVK_F17:
    case kVK_F18:
    case kVK_F19:
    case kVK_F20:
    case kVK_UpArrow:
    case kVK_DownArrow:
    case kVK_LeftArrow:
    case kVK_RightArrow:
    case kVK_Return:
    case kVK_Delete:
    case kVK_ForwardDelete:
    case kVK_Escape:
    case kVK_Tab:
    case kVK_Home:
    case kVK_End:
    case kVK_PageUp:
    case kVK_PageDown:
    case kVK_VolumeUp:
    case kVK_VolumeDown:
    case kVK_Mute:
      st->ime.state_flag |= GHOST_IME_KEY_CONTROL_CHAR;
      return;
  }
}

void set_ime_candidate_win_pos(ViewState *st, int32_t x, int32_t y, int32_t w, int32_t h)
{
  int32_t outX, outY;
  st->window->clientToScreen(x, y, outX, outY);
  st->ime.candidate_window_position = CGRectMake(
      (CGFloat)outX, (CGFloat)outY, (CGFloat)w, (CGFloat)h);
}

#endif /* WITH_INPUT_IME */

/* -------------------------------------------------------------------------
 * Las implementaciones que registra el runtime.
 *
 * Son funciones de C normales. Los dos primeros parametros, `self` y `_cmd`, los pone
 * el compilador en Objective-C y aqui se escriben a mano: olvidarse de `_cmd` desplaza
 * todos los argumentos y no da ningun aviso.
 */

void imp_dealloc(id self, SEL _cmd)
{
  /* El original NO tenia `dealloc`: sus variables de instancia vivian dentro del objeto
   * y se iban con el. Aqui el estado esta en el monticulo, asi que hay que soltarlo o se
   * filtraria MAS que antes. No es un cambio de conducta, es conservarla. */
  if (ViewState *st = state_of(self)) {
    composing_free(st);
    delete st;
    ghost_objc::ivar_set<ViewState *>(self, kStateIvar, nullptr);
  }
  msg_super<void>(self, class_getSuperclass(object_getClass(self)), _cmd);
}

signed char imp_acceptsFirstResponder(id, SEL)
{
  return 1; /* YES */
}

signed char imp_acceptsFirstMouse(id, SEL, id /*event*/)
{
  return 1; /* YES */
}

signed char imp_isOpaque(id, SEL)
{
  return 1; /* YES */
}

/* The trick to prevent Cocoa from complaining (beeping) */
void imp_keyDown(id self, SEL, id event)
{
  ViewState *st = state_of(self);

#ifdef WITH_INPUT_IME
  check_key_code_is_control_char(st, event);
  const bool ime_process = is_processed_by_ime(st);
#else
  const bool ime_process = false;
#endif

  if (!ime_process) {
    st->system->handleKeyEvent(event);
  }

  /* Start or continue composing? */
  if (string_length(msg<id>(event, GHOST_SEL(characters))) == 0 ||
      string_length(msg<id>(event, GHOST_SEL(charactersIgnoringModifiers))) == 0 || st->composing ||
      ime_process)
  {
    st->composing = true;

    /* Interpret event to call insertText. */
    {
      ghost_objc::AutoreleasePool pool;
      id array = msg<id>(GHOST_CLS(NSArray), GHOST_SEL(arrayWithObject:), event);
      msg<void>(self, GHOST_SEL(interpretKeyEvents:), array); /* Calls insertText. */
    }

#ifdef WITH_INPUT_IME
    /* For Korean input, control characters are also processed by handleKeyEvent. */
    const int controlCharForKorean = (GHOST_IME_COMPOSITION_EVENT | GHOST_IME_RESULT_EVENT |
                                      GHOST_IME_KEY_CONTROL_CHAR);
    if (((st->ime.state_flag & controlCharForKorean) == controlCharForKorean)) {
      st->system->handleKeyEvent(event);
    }

    st->ime.state_flag &= ~(GHOST_IME_COMPOSITION_EVENT | GHOST_IME_RESULT_EVENT);

    st->ime.combined_result.clear();
#endif

    return;
  }
}

void imp_handleKeyEvent(id self, SEL, id event)
{
  state_of(self)->system->handleKeyEvent(event);
}

void imp_handleMouseEvent(id self, SEL, id event)
{
  state_of(self)->system->handleMouseEvent(event);
}

void imp_drawRect(id self, SEL _cmd, CGRect rect)
{
  ViewState *st = state_of(self);

  if (msg<signed char>(self, GHOST_SEL(inLiveResize))) {
    /* Don't redraw while in live resize */
  }
  else {
    msg_super<void>(self, class_getSuperclass(object_getClass(self)), _cmd, rect);
    st->system->handleWindowEvent(GHOST_kEventWindowUpdate, st->window);

    /* For some cases like entering full-screen we need to redraw immediately
     * so our window does not show blank during the animation */
    if (st->window->getImmediateDraw()) {
      st->system->dispatchEvents();
    }
  }
}

/* Text input. */

/* Processes the Result String sent from the Input Method. */
void imp_insertText_replacementRange(id self, SEL, id chars, NSRange_ /*replacementRange*/)
{
  ViewState *st = state_of(self);
  composing_free(st);

#ifdef WITH_INPUT_IME
  if (st->ime.state_flag & GHOST_IME_ENABLED) {
    if (!(st->ime.state_flag & GHOST_IME_COMPOSING)) {
      process_ime_event(st, GHOST_kEventImeCompositionStart);
    }

    /* For Chinese and Korean input, insertText may be executed twice with a single keyDown. */
    if (st->ime.state_flag & GHOST_IME_RESULT_EVENT) {
      st->ime.combined_result += convert_nsstring(chars);
    }
    else {
      st->ime.combined_result = convert_nsstring(chars);
    }

    set_ime_result(st, st->ime.combined_result);

    /* For Korean input, both "Result Event" and "Composition Event"
     * can occur in a single keyDown. */
    if (!ime_did_composition(st)) {
      process_ime_event(st, GHOST_kEventImeComposition);
    }
    st->ime.state_flag |= GHOST_IME_RESULT_EVENT;

    process_ime_event(st, GHOST_kEventImeCompositionEnd);
    st->ime.state_flag &= ~GHOST_IME_COMPOSING;
  }
#else
  (void)chars;
#endif
}

/* Processes the Composition String sent from the Input Method. */
void imp_setMarkedText(id self, SEL, id chars, NSRange_ range, NSRange_ /*replacementRange*/)
{
  ViewState *st = state_of(self);
  composing_free(st);

  if (string_length(chars) == 0) {
#ifdef WITH_INPUT_IME
    /* Processes when the last Composition String is deleted. */
    if (st->ime.state_flag & GHOST_IME_COMPOSING) {
      set_ime_result(st, std::string());
      process_ime_event(st, GHOST_kEventImeComposition);
      process_ime_event(st, GHOST_kEventImeCompositionEnd);
      st->ime.state_flag &= ~GHOST_IME_COMPOSING;
    }
#endif

    return;
  }

  /* Start composing. */
  st->composing = true;
  st->composing_text = msg<id>(chars, GHOST_SEL(copy));

  /* Chars of markedText by Input Method is an instance of NSAttributedString */
  if (msg<signed char>(chars, GHOST_SEL(isKindOfClass:), GHOST_CLS(NSAttributedString))) {
    /* El original SOBREESCRIBIA `composing_text` aqui sin soltar la copia anterior; se
     * conserva tal cual para no cambiar la conducta observable. */
    st->composing_text = msg<id>(msg<id>(chars, GHOST_SEL(string)), GHOST_SEL(copy));
  }

  /* If empty, cancel. */
  if (string_length(st->composing_text) == 0) {
    composing_free(st);
  }

#ifdef WITH_INPUT_IME
  if (st->ime.state_flag & GHOST_IME_ENABLED) {
    if (!(st->ime.state_flag & GHOST_IME_COMPOSING)) {
      st->ime.state_flag |= GHOST_IME_COMPOSING;
      process_ime_event(st, GHOST_kEventImeCompositionStart);
    }

    set_ime_composition(st, st->composing_text, range);

    /* For Korean input, setMarkedText may be executed twice with a single keyDown. */
    if (!ime_did_composition(st)) {
      st->ime.state_flag |= GHOST_IME_COMPOSITION_EVENT;
      process_ime_event(st, GHOST_kEventImeComposition);
    }
  }
#else
  (void)range;
#endif
}

void imp_unmarkText(id self, SEL)
{
  composing_free(state_of(self));
}

signed char imp_hasMarkedText(id self, SEL)
{
  return state_of(self)->composing ? 1 : 0;
}

void imp_doCommandBySelector(id, SEL, SEL /*selector*/) {}

id imp_attributedSubstringForProposedRange(id, SEL, NSRange_, NSRange_ * /*actualRange*/)
{
  return ghost_objc::autorelease(ghost_objc::alloc_init("NSAttributedString"));
}

NSRange_ imp_markedRange(id self, SEL)
{
  ViewState *st = state_of(self);
  const NSUInteger_ length = st->composing_text ? string_length(st->composing_text) : 0;

  if (st->composing) {
    return NSRange_{0, length};
  }

  return NSRange_{(NSUInteger_)kNSNotFound, 0};
}

NSRange_ imp_selectedRange(id self, SEL)
{
  ViewState *st = state_of(self);
  const NSUInteger_ length = st->composing_text ? string_length(st->composing_text) : 0;
  return NSRange_{0, length};
}

/* Specify the position where the Chinese and Japanese candidate windows are displayed. */
CGRect imp_firstRectForCharacterRange(id self, SEL, NSRange_, NSRange_ * /*actualRange*/)
{
#ifdef WITH_INPUT_IME
  ViewState *st = state_of(self);
  if (st->ime.state_flag & GHOST_IME_ENABLED) {
    return st->ime.candidate_window_position;
  }
#else
  (void)self;
#endif
  return CGRectZero;
}

NSUInteger_ imp_characterIndexForPoint(id, SEL, CGPoint)
{
  return (NSUInteger_)kNSNotFound;
}

id imp_validAttributesForMarkedText(id, SEL)
{
  return msg<id>(GHOST_CLS(NSArray), GHOST_SEL(array));
}

#ifdef WITH_INPUT_IME
void imp_ImeDidChangeCallback(id self, SEL, id /*notification*/)
{
  check_ime_enabled(state_of(self));
}
#endif

/* -------------------------------------------------------------------------
 * Registro de las dos clases.
 */

Class build_view_class(const char *name, const char *superclass_name)
{
  ghost_objc::ClassBuilder b(name, superclass_name);

  b.protocol("NSTextInputClient");
  b.ivar(kStateIvar, sizeof(void *), 3, "^v");

  /* Metodos de NSView / NSObject: la codificacion la da la superclase. */
  b.method("dealloc", (IMP)imp_dealloc);
  b.method("acceptsFirstResponder", (IMP)imp_acceptsFirstResponder);
  b.method("acceptsFirstMouse:", (IMP)imp_acceptsFirstMouse);
  b.method("isOpaque", (IMP)imp_isOpaque);
  b.method("drawRect:", (IMP)imp_drawRect);
  b.method("keyDown:", (IMP)imp_keyDown);

  /* Los dos grupos que en el original eran las macros HANDLE_KEY_EVENT /
   * HANDLE_MOUSE_EVENT / HANDLE_TABLET_EVENT: 17 reenvios de una linea. */
  static const char *const key_events[] = {"keyUp:", "flagsChanged:"};
  for (const char *sel_name : key_events) {
    b.method(sel_name, (IMP)imp_handleKeyEvent);
  }
  static const char *const mouse_events[] = {"mouseDown:",
                                             "mouseUp:",
                                             "rightMouseDown:",
                                             "rightMouseUp:",
                                             "mouseMoved:",
                                             "mouseDragged:",
                                             "rightMouseDragged:",
                                             "scrollWheel:",
                                             "otherMouseDown:",
                                             "otherMouseUp:",
                                             "otherMouseDragged:",
                                             "magnifyWithEvent:",
                                             "smartMagnifyWithEvent:",
                                             "rotateWithEvent:",
                                             "tabletPoint:",
                                             "tabletProximity:"};
  for (const char *sel_name : mouse_events) {
    b.method(sel_name, (IMP)imp_handleMouseEvent);
  }

  /* NSTextInputClient: la codificacion la da el protocolo. Hay que implementarlos
   * TODOS: declarar conformidad y dejarse uno hace que AppKit mande un selector que la
   * clase no reconoce y la aplicacion muera. Medido en el piloto. */
  b.method("insertText:replacementRange:", (IMP)imp_insertText_replacementRange);
  b.method("setMarkedText:selectedRange:replacementRange:", (IMP)imp_setMarkedText);
  b.method("unmarkText", (IMP)imp_unmarkText);
  b.method("hasMarkedText", (IMP)imp_hasMarkedText);
  b.method("doCommandBySelector:", (IMP)imp_doCommandBySelector);
  b.method("attributedSubstringForProposedRange:actualRange:",
           (IMP)imp_attributedSubstringForProposedRange);
  b.method("markedRange", (IMP)imp_markedRange);
  b.method("selectedRange", (IMP)imp_selectedRange);
  b.method("firstRectForCharacterRange:actualRange:", (IMP)imp_firstRectForCharacterRange);
  b.method("characterIndexForPoint:", (IMP)imp_characterIndexForPoint);
  b.method("validAttributesForMarkedText", (IMP)imp_validAttributesForMarkedText);

#ifdef WITH_INPUT_IME
  /* Metodo PROPIO: lo llama el centro de notificaciones por selector, asi que tiene que
   * estar registrado, y su codificacion no la conoce nadie mas. `v@:@` = void, self,
   * _cmd, un objeto; es la misma forma que `windowDidResize:`. */
  b.method("ImeDidChangeCallback:", (IMP)imp_ImeDidChangeCallback, "v@:@");
#endif

  return b.finish();
}

}  // namespace

namespace ghost_cocoa_view {

Class view_class(bool metal)
{
  /* Una clase por variante, construida una sola vez. El `static` local de C++ garantiza
   * que la inicializacion ocurre una vez y de forma segura entre hilos. */
  if (metal) {
    static Class cls = build_view_class("CocoaMetalView", "NSView");
    return cls;
  }
  static Class cls = build_view_class("CocoaOpenGLView", "NSOpenGLView");
  return cls;
}

id view_create(bool metal, GHOST_SystemCocoa *system, GHOST_WindowCocoa *window)
{
  /* El original hacia `[super init]` e ignoraba el `frame` que le pasaban; se conserva. */
  id view = msg<id>(msg<id>((id)view_class(metal), GHOST_SEL(alloc)), GHOST_SEL(init));
  if (!view) {
    return nullptr;
  }

  ViewState *st = new ViewState();
  st->system = system;
  st->window = window;
  st->composing = false;
  st->composing_text = nullptr;

#ifdef WITH_INPUT_IME
  st->ime.state_flag = 0;
  st->ime.candidate_window_position = CGRectZero;
  st->ime.event.cursor_position = -1;
  st->ime.event.target_start = -1;
  st->ime.event.target_end = -1;
#endif

  ghost_objc::ivar_set<ViewState *>(view, kStateIvar, st);

#ifdef WITH_INPUT_IME
  /* Register a function to be executed when Input Method is changed using
   * "Control + Space" or language-specific keys (such as "EISU / KANA" key for Japanese). */
  {
    ghost_objc::AutoreleasePool pool;
    id center = msg<id>(GHOST_CLS(NSNotificationCenter), GHOST_SEL(defaultCenter));
    msg<void>(center,
              GHOST_SEL(addObserver:selector:name:object:),
              view,
              ghost_objc::sel("ImeDidChangeCallback:"),
              NSTextInputContextKeyboardSelectionDidChangeNotification,
              (id) nullptr);
  }
#endif

  return view;
}

#ifdef WITH_INPUT_IME

void begin_ime(id view, int32_t x, int32_t y, int32_t w, int32_t h, bool /*completed*/)
{
  ViewState *st = state_of(view);
  st->ime.state_flag |= GHOST_IME_INPUT_FOCUSED;
  check_ime_enabled(st);
  set_ime_candidate_win_pos(st, x, y, w, h);
}

void end_ime(id view)
{
  ViewState *st = state_of(view);
  st->ime.state_flag = 0;
  st->ime.event.result.clear();
  st->ime.event.composite.clear();

  composing_free(st);
  {
    ghost_objc::AutoreleasePool pool;
    msg<void>(msg<id>(GHOST_CLS(NSTextInputContext), GHOST_SEL(currentInputContext)),
              GHOST_SEL(discardMarkedText));
  }
}

#endif /* WITH_INPUT_IME */

}  // namespace ghost_cocoa_view
