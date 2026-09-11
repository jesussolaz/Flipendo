/*
 * ***** BEGIN GPL LICENSE BLOCK *****
 *
 * This program is free software; you can redistribute it and/or
 * modify it under the terms of the GNU General Public License
 * as published by the Free Software Foundation; either version 2
 * of the License, or (at your option) any later version.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 *
 * You should have received a copy of the GNU General Public License
 * along with this program; if not, write to the Free Software Foundation,
 * Inc., 51 Franklin Street, Fifth Floor, Boston, MA 02110-1301, USA.
 *
 * ***** END GPL LICENSE BLOCK *****
 */

/** \file gameengine/Rasterizer/RAS_ICanvas.cpp
 *  \ingroup bgerast
 */

#include "RAS_ICanvas.hpp"

#include <cstdlib>
#include <utility>
#include <vector>

#include "BKE_image.hh"
#include "BKE_image_format.hh"
#include "BLI_path_utils.hh"
#include "BLI_string.h"
#include "BLI_task.h"
#include "DNA_scene_types.h"
#include "IMB_imbuf.hh"
#include "IMB_imbuf_types.hh"
#include "MEM_guardedalloc.h"

#include "CM_Message.hpp"

// Task data for saving screenshots in a different thread.
struct ScreenshotTaskData {
  unsigned int *dumprect;
  int dumpsx;
  int dumpsy;
  char path[FILE_MAX];
  ImageFormatData *im_format;
};

/**
 * Function that actually performs the image compression and saving to disk of a screenshot.
 * Run in a separate thread by RAS_ICanvas::save_screenshot().
 *
 * @param taskdata Must point to a ScreenshotTaskData object. This function takes ownership
 *                 of all pointers in the ScreenshotTaskData, and frees them.
 */
void save_screenshot_thread_func(TaskPool *__restrict pool, void *taskdata, int threadid);

RAS_ICanvas::RAS_ICanvas(RAS_Rasterizer *rasty)
    : m_rasterizer(rasty), m_samples(0), m_mousestate(MOUSE_NORMAL), m_frame(0)
{
  /* Flipendo: `m_frame` se usaba sin inicializar en SaveScreeshot (lo lee
   * `BLI_path_frame()` para resolver los `#` de la ruta). Era comportamiento
   * indefinido, y con una ruta con `#` la captura acababa con un numero de basura. */
  m_taskpool = BLI_task_pool_create(nullptr, TASK_PRIORITY_LOW);
}

RAS_ICanvas::~RAS_ICanvas()
{
  /* Flipendo: una captura encolada que se queda sin volcar porque el juego se cierra
   * era la trampa escrita en politicas/UI-JUEGO-NATIVA.md seccion 11: «pedirla y salir
   * en el mismo tic la deja sin escribir MIENTRAS EL LOG DICE QUE SE HA CAPTURADO».
   * Aqui se acaba el silencio: si queda alguna en la cola, se dice cual. */
  for (const Screenshot &screenshot : m_screenshots) {
    CM_Error("ARNES: la captura '" << screenshot.path
                                   << "' se quedo en la cola y el juego se cerro antes de "
                                      "volcarla: no hay fichero. La captura se vuelca en "
                                      "EndFrame, asi que hay que dejar correr unos "
                                      "fotogramas antes de salir.");
    MEM_freeN(screenshot.format);
  }
  m_screenshots.clear();

  if (m_taskpool) {
    BLI_task_pool_work_and_wait(m_taskpool);
    BLI_task_pool_free(m_taskpool);
    m_taskpool = nullptr;
  }
}

void RAS_ICanvas::SetSamples(int samples)
{
  m_samples = samples;
}

int RAS_ICanvas::GetSamples() const
{
  return m_samples;
}

int RAS_ICanvas::GetWidth() const
{
  return m_viewportArea.GetWidth();
}

int RAS_ICanvas::GetHeight() const
{
  return m_viewportArea.GetHeight();
}

float RAS_ICanvas::GetMouseNormalizedX(int x)
{
  return float(x) / GetWidth();
}

float RAS_ICanvas::GetMouseNormalizedY(int y)
{
  return float(y) / GetHeight();
}

const RAS_Rect &RAS_ICanvas::GetWindowArea() const
{
  return m_windowArea;
}

const RAS_Rect &RAS_ICanvas::GetViewportArea() const
{
  return m_viewportArea;
}

void RAS_ICanvas::FlushScreenshots()
{
  /* Flipendo: una captura cuya lectura de la GPU no escribio nada NO se escribe a
   * disco: se vuelve a encolar para el fotograma siguiente, y cuando se acaban los
   * intentos se declara el fallo en voz alta. Antes se guardaba el buffer intacto como
   * un PNG valido y el motor decia que habia capturado. */
  std::vector<Screenshot> retry;

  for (Screenshot &screenshot : m_screenshots) {
    if (SaveScreeshot(screenshot)) {
      continue;
    }
    screenshot.attempts--;
    if (screenshot.attempts > 0) {
      /* Se dice en voz alta: sin esto no hay forma de saber si una captura salio a la
       * primera o se salvo por el reintento, y esa diferencia es justo lo que hay que
       * medir para saber si el reintento sirve de algo. */
      CM_Warning("la captura '" << screenshot.path
                                << "' no leyo ni un pixel; se reintenta en el fotograma "
                                   "siguiente (quedan "
                                << screenshot.attempts << " intentos).");
      retry.push_back(screenshot);
      continue;
    }
    CM_Error("ARNES: la captura '"
             << screenshot.path << "' se descarta tras " << SCREENSHOT_ATTEMPTS
             << " intentos: la lectura del buffer trasero no devolvio ni un pixel. "
                "Causa conocida en macOS/Metal: hay OTRO Blenderplayer abierto a la vez. "
                "Captura de uno en uno; no se escribe ningun fichero.");
    /* El formato era del llamante mientras quedaban intentos; aqui ya no lo es. */
    MEM_freeN(screenshot.format);
  }

  m_screenshots = std::move(retry);
}

void RAS_ICanvas::AddScreenshot(
    const std::string &path, int x, int y, int width, int height, ImageFormatData *format)
{
  Screenshot screenshot;
  screenshot.path = path;
  screenshot.x = x;
  screenshot.y = y;
  screenshot.width = width;
  screenshot.height = height;
  screenshot.format = format;
  screenshot.attempts = SCREENSHOT_ATTEMPTS;

  m_screenshots.push_back(screenshot);
}

void save_screenshot_thread_func(TaskPool *__restrict (pool),
                                 void *taskdata,
                                 int /*(threadid)*/)
{
  ScreenshotTaskData *task = static_cast<ScreenshotTaskData *>(taskdata);

  /* create and save imbuf */
  ImBuf *ibuf = IMB_allocImBuf(task->dumpsx, task->dumpsy, 24, 0);
  ibuf->byte_buffer.data = (uint8_t *)task->dumprect;

  BKE_imbuf_write_as(ibuf, task->path, task->im_format, false);

  ibuf->byte_buffer.data = nullptr;
  IMB_freeImBuf(ibuf);
  // Dumprect is allocated in RAS_OpenGLRasterizer::MakeScreenShot with malloc(), we must use
  // free() then.
  free(task->dumprect);
  MEM_freeN(task->im_format);
}

bool RAS_ICanvas::SaveScreeshot(const Screenshot &screenshot)
{
  unsigned int *pixels = m_rasterizer->MakeScreenshot(
      screenshot.x, screenshot.y, screenshot.width, screenshot.height);
  if (!pixels) {
    CM_Error("cannot allocate pixels array");
    return true; /* No hay nada que reintentar: fallo de memoria, no de lectura. */
  }

  /* Flipendo: cuantos bytes siguen siendo el centinela, es decir, cuantos NO escribio
   * la GPU. Todos => la lectura no hizo nada y esto no es una captura; algunos => la
   * lectura se quedo a medias y hay que verlo, pero el fichero se escribe igual para
   * poder mirarlo. */
  const size_t bytes = sizeof(unsigned int) * size_t(screenshot.width) *
                       size_t(screenshot.height);
  const unsigned char *raw = reinterpret_cast<const unsigned char *>(pixels);
  size_t unread = 0;
  for (size_t i = 0; i < bytes; i++) {
    unread += (raw[i] == RAS_Rasterizer::SCREENSHOT_UNREAD_BYTE) ? 1 : 0;
  }
  if (unread == bytes) {
    free(pixels);
    return false;
  }
  /* El umbral no es cosmetico: una captura de verdad trae por azar ~1 de cada 256
   * bytes con el valor 0xCD (unos 1.900 de 480.000 en 400x300), asi que avisar con
   * `unread > 0` seria un aviso en CADA captura. Por encima de la mitad del buffer no
   * puede ser azar. */
  if (unread * 2 > bytes) {
    CM_Warning("la captura '" << screenshot.path << "' se leyo a medias: " << unread << " de "
                              << bytes << " bytes se quedaron sin escribir por la GPU.");
  }

  /* Save the actual file in a different thread, so that the
   * game engine can keep running at full speed. */
  ScreenshotTaskData *task = (ScreenshotTaskData *)MEM_mallocN(sizeof(ScreenshotTaskData),
                                                               "screenshot-data");
  task->dumprect = pixels;
  task->dumpsx = screenshot.width;
  task->dumpsy = screenshot.height;
  task->im_format = screenshot.format;

  BLI_strncpy(task->path, screenshot.path.c_str(), FILE_MAX);
  BLI_path_frame(task->path, sizeof(task->path), m_frame, 0);
  m_frame++;
  BKE_image_path_ext_from_imtype_ensure(task->path, sizeof(task->path), task->im_format->imtype);

  BLI_task_pool_push(m_taskpool,
                     (TaskRunFunction)save_screenshot_thread_func,
                     task,
                     true,  // free task data
                     NULL);
  return true;
}
