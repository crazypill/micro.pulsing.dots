//
//  pulsing_dots.h
//  
//
//  Created by Alex Lelievre on 1/2/19.
//

#ifndef pulsing_dots_h
#define pulsing_dots_h

#include <stdio.h>
#include <Arduino.h>


// Defines -----------------------------------------------------------------

//#define TWO_DISPLAYS  // change this to a variable !!@


static const int8_t   kDeviceWidth    = 16;
static const int8_t   kDeviceHeight   = 9;

static const uint32_t kFrameDelayMS   = 0;
static const uint8_t  kMaxBrightness  = 220;
static const uint8_t  kOverBrightness = 255;

static const uint32_t kNumSteps       = 600;
static const int8_t   kMaxWidth       = 16;

#ifdef TWO_DISPLAYS
static const uint8_t  kMaxDots           = 200;
static const uint8_t  kNumDots           = kMaxDots;
static const int8_t   kMaxHeight         = kDeviceHeight * 2;            // 2 devices stacked
#else
static const uint8_t  kMaxDots           = 100;
static const uint8_t  kNumDots           = kMaxDots;
static const int8_t   kMaxHeight         = kDeviceHeight;
#endif


// Data types -----------------------------------------------------------------

enum
{
  kNorth,
  kNE,
  kEast,
  kSE,
  kSouth,
  kSW,
  kWest,
  kNW,

  kDirectionCount // please leave last
};


typedef struct
{
  uint8_t  x;
  uint8_t  y;
  uint32_t step;
  uint32_t num_steps;
  uint8_t  max_brightness;
} PulseState;


// Public API -----------------------------------------------------------------

void     pulsing_dots_setup(); 
uint8_t* pulsing_dots_get_render_buffer();
void     pulsing_dots_draw( float accel_x, float accel_y, float accel_z, bool erase );

 
#endif // pulsing_dots_h
// EOF
