/*
  Created by Fabrizio Di Vittorio (fdivitto2013@gmail.com) - <http://www.fabgl.com>
  Copyright (c) 2019-2022 Fabrizio Di Vittorio.
  All rights reserved.


* Please contact fdivitto2013@gmail.com if you need a commercial license.


* This library and related software is available under GPL v3.

  FabGL is free software: you can redistribute it and/or modify
  it under the terms of the GNU General Public License as published by
  the Free Software Foundation, either version 3 of the License, or
  (at your option) any later version.

  FabGL is distributed in the hope that it will be useful,
  but WITHOUT ANY WARRANTY; without even the implied warranty of
  MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
  GNU General Public License for more details.

  You should have received a copy of the GNU General Public License
  along with FabGL.  If not, see <http://www.gnu.org/licenses/>.
 */


#pragma once



/**
 * @file
 *
 * @brief This file contains fabgl::CVBSPalettedController definition.
 */


#include <stdint.h>
#include <stddef.h>
#include <atomic>

#include "freertos/FreeRTOS.h"
#include "freertos/queue.h"

#include "fabglconf.h"
#include "fabutils.h"
#include "displaycontroller.h"
#include "dispdrivers/cvbsbasecontroller.h"




namespace fabgl {




class CVBSPalettedController : public CVBSBaseController {

public:

  CVBSPalettedController(int columnsQuantum, NativePixelFormat nativePixelFormat, int viewPortRatioDiv, int viewPortRatioMul);
  ~CVBSPalettedController();

  // unwanted methods
  CVBSPalettedController(CVBSPalettedController const&) = delete;
  void operator=(CVBSPalettedController const&)        = delete;

  void end();

  // abstract method of BitmappedDisplayController
  void suspendBackgroundPrimitiveExecution();

  // import "modeline" version of setResolution
  using CVBSBaseController::setResolution;

  void setResolution(CVBSParams const * params, int viewPortWidth = -1, int viewPortHeight = -1, bool doubleBuffered = false);

  int getPaletteSize();

  virtual int colorsCount()      { return getPaletteSize(); }

  void setProcessPrimitivesOnBlank(bool value)          { m_processPrimitivesOnBlank = value; }

  // returns "static" version of m_viewPort
  static uint8_t * sgetScanline(int y)                  { return (uint8_t*) s_viewPort[y]; }

  // abstract method of BitmappedDisplayController
  NativePixelFormat nativePixelFormat()                 { return m_nativePixelFormat; }
  
  virtual bool suspendDoubleBuffering(bool value);

protected:

  void init();

  virtual void setupDefaultPalette() = 0;

  void updateRGB2PaletteLUT();
  void calculateAvailableCyclesForDrawings();
  static void primitiveExecTask(void * arg);

  uint8_t RGB888toPaletteIndex(RGB888 const & rgb) {
    return m_packedRGB222_to_PaletteIndex[RGB888toPackedRGB222(rgb)];
  }

  uint8_t RGB2222toPaletteIndex(uint8_t value) {
    return m_packedRGB222_to_PaletteIndex[value & 0b00111111];
  }

  uint8_t RGB8888toPaletteIndex(RGBA8888 value) {
    return RGB888toPaletteIndex(RGB888(value.R, value.G, value.B));
  }

  // abstract method of BitmappedDisplayController
  void swapBuffers();


  TaskHandle_t                m_primitiveExecTask;

  // optimization: clones of m_viewPort and m_viewPortVisible
  static volatile uint8_t * * s_viewPort;
  static volatile uint8_t * * s_viewPortVisible;

  RGB222 *                    m_palette;


protected:

  void allocateViewPort();
  void freeViewPort();
  void checkViewPortSize();

  // Maximum time (in CPU cycles) available for primitives drawing
  volatile uint32_t           m_primitiveExecTimeoutCycles;

  volatile bool               m_taskProcessingPrimitives;

  // true = allowed time to process primitives is limited to the vertical blank. Slow, but avoid flickering
  // false = allowed time is the half of an entire frame. Fast, but may flick
  bool                        m_processPrimitivesOnBlank;

  uint8_t                     m_packedRGB222_to_PaletteIndex[64];

  // configuration
  int                         m_columnsQuantum; // viewport width must be divisble by m_columnsQuantum
  NativePixelFormat           m_nativePixelFormat;
  int                         m_viewPortRatioDiv;
  int                         m_viewPortRatioMul;

};



} // end of namespace








