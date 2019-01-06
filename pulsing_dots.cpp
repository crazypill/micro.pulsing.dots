//
//  pulsing_dots.cpp
//  
//
//  Created by Alex Lelievre on 1/2/19.
//

#include "pulsing_dots.h"


// Defines -----------------------------------------------------------------

#define roundFloat( v ) (uint32_t)(v + 0.5f)

//#define ALLOW_DOTS_TO_DISAPPEAR   // makes it so that the dark spots caused by shifting aren't filled in randomly
//#define RANDOM_DURATION           // makes it more shimmery by allowing the number of steps in pulsing to be random
//#define DUMP_PULSE



// Constants and static data----------------------------------------------------

static const int8_t   kFlashDelayMS   = 100;
static const uint8_t  kMinDotSteps    = 3;

#ifdef TWO_DISPLAYS
static uint8_t        s_image_buffer[18 * 16];                           // Buffer for rendering image (2x 9 * 16 buffers)
static uint8_t*       s_buffer_ptr       = &s_image_buffer[0];           // Current pointer into buffer data
static uint8_t*       s_buffer_ptr2      = &s_image_buffer[144];         // Current pointer into second screen data
#else
static uint8_t        s_image_buffer[9 * 16];                            // Buffer for rendering image
static uint8_t*       s_buffer_ptr       = &s_image_buffer[0];           // Current pointer into buffer data
#endif

static uint8_t        s_frame = 0;
static PulseState     s_dot[kMaxDots] = {0};

// used to make LED brightness more linear
static const uint8_t PROGMEM s_gamma_table[] =
{
    0,  0,  0,  0,  0,  0,  0,  0,  0,  0,  0,  0,  0,  0,  0,  0,
    0,  0,  0,  0,  0,  0,  0,  0,  0,  0,  0,  0,  1,  1,  1,  1,
    1,  1,  1,  1,  1,  1,  1,  1,  1,  2,  2,  2,  2,  2,  2,  2,
    2,  3,  3,  3,  3,  3,  3,  3,  4,  4,  4,  4,  4,  5,  5,  5,
    5,  6,  6,  6,  6,  7,  7,  7,  7,  8,  8,  8,  9,  9,  9, 10,
    10, 10, 11, 11, 11, 12, 12, 13, 13, 13, 14, 14, 15, 15, 16, 16,
    17, 17, 18, 18, 19, 19, 20, 20, 21, 21, 22, 22, 23, 24, 24, 25,
    25, 26, 27, 27, 28, 29, 29, 30, 31, 32, 32, 33, 34, 35, 35, 36,
    37, 38, 39, 39, 40, 41, 42, 43, 44, 45, 46, 47, 48, 49, 50, 50,
    51, 52, 54, 55, 56, 57, 58, 59, 60, 61, 62, 63, 64, 66, 67, 68,
    69, 70, 72, 73, 74, 75, 77, 78, 79, 81, 82, 83, 85, 86, 87, 89,
    90, 92, 93, 95, 96, 98, 99,101,102,104,105,107,109,110,112,114,
    115,117,119,120,122,124,126,127,129,131,133,135,137,138,140,142,
    144,146,148,150,152,154,156,158,160,162,164,167,169,171,173,175,
    177,180,182,184,186,189,191,193,196,198,200,203,205,208,210,213,
    215,218,220,223,225,228,231,233,236,239,241,244,247,249,252,255
};


// Private API -----------------------------------------------------------------

uint8_t gamma( uint8_t input );

void draw_pixel( uint8_t* buff, uint8_t x, uint8_t y, uint8_t intensity );
void draw_dot( uint8_t* buff, uint8_t x, uint8_t y, uint8_t intensity );

void draw_pulse( uint8_t* buff, PulseState* state );
void move_dot_using_accel( PulseState* state, float x, float y, float z );
void move_dot_randomly( PulseState* state );

void cloud( uint8_t* buff );
void blob( uint8_t* buff );
void disappearing( uint8_t* buff );
void disappearing_accel( uint8_t* buff, float x, float y, float z );
void blob_accel( uint8_t* buff, float x, float y, float z );
void all_on_low( uint8_t* buff );



// Code -----------------------------------------------------------------

#pragma mark -


// gamma function corrects for non-linear display output that is life
uint8_t gamma( uint8_t input )
{
    // !!@ I wonder about the speed up if we move this table to RAM instead of Flash !!@
    return pgm_read_byte( &s_gamma_table[input] );
}


void draw_pixel( uint8_t* buff, uint8_t x, uint8_t y, uint8_t intensity )
{
  // don't draw outside buffer
  if( x < 0 || x >= kMaxWidth )
    return;
  
  if( y < 0 || y >= kMaxHeight )
    return;

  // simply index into the buffer
  buff[y * kMaxWidth + x] = gamma( intensity );
}


void draw_dot( uint8_t* buff, uint8_t x, uint8_t y, uint8_t intensity )
{
  // we draw a pixel at full intensity and four around it, like a star (or pixelated circle)
  draw_pixel( buff, x, y, intensity );

  // half intensity pixels
  draw_pixel( buff, x, y + 1, intensity / 2 );
  draw_pixel( buff, x, y - 1, intensity / 2 );
  draw_pixel( buff, x + 1, y, intensity / 2 );
  draw_pixel( buff, x - 1, y, intensity / 2 );
}



void draw_pulse( uint8_t* buff, PulseState* state )
{
    uint32_t half = roundFloat( state->num_steps * 0.5f );

#ifdef DUMP_PULSE
    Serial.print( "draw_pulse: half: " );
    Serial.print( half );
    Serial.print( ", step: " );
    Serial.print( state->step );
#endif
    
    // ramp up halfway and then ramp down
    if( state->step < half )
    {
      uint32_t maxValue = state->step + 1;
      draw_dot( buff, state->x, state->y, state->max_brightness - roundFloat( state->max_brightness / maxValue ) );
#ifdef DUMP_PULSE
      Serial.print( ", up: " );
      Serial.print( state->max_brightness - roundFloat( state->max_brightness / maxValue ) );
#endif
    }
    else
    {
      uint32_t i = (state->step - half);
      uint32_t maxValue = i + 1;
      uint8_t intensity = i == 0 ? state->max_brightness : roundFloat( state->max_brightness / maxValue );
      draw_dot( buff, state->x, state->y, intensity );
#ifdef DUMP_PULSE
      Serial.print( ", dn: " );
      Serial.print( intensity );
#endif
    }

    ++state->step;
    if( state->step >= state->num_steps )
      state->step = 0;
      
#ifdef DUMP_PULSE
    Serial.println();
#endif
}

#pragma mark -

void move_dot_using_accel( PulseState* state, float x, float y, float z )
{
   // we use rounding to turn almost zero values to zero.  The scale is 0-9 or so...
  state->x = roundFloat( x + state->x );    // what should we do with z coord?
  state->y = roundFloat( y + state->y );

#ifdef ALLOW_DOTS_TO_DISAPPEAR
  // now make sure this dot still fits in the screen (eventually when we draw the dot ourselves we can let it clip)
  if( state->x < 1 )
    state->x = 1;
  if( state->y < 1)
    state->y = 1;

  if( state->x >= kMaxWidth )
    state->x = kMaxWidth - 1;
  if( state->y >= kMaxHeight )
    state->y = kMaxHeight - 1;
#else
  // if the dot disappeared, make it randomly appear again -- note: we should only 
  // allow the new dot to be in the invalidated area, not the entire display !!@
  if( state->x < 1 || state->x > kMaxWidth || state->y < 1 || state->y > kMaxHeight )
  {
    state->x = random( 0, kMaxWidth );
    state->y = random( 0, kMaxHeight );
  }
#endif  
}

void move_dot_randomly( PulseState* state )
{
  // pick a random direction and then move just one pixel that way
  uint8_t randDirection = random( 0, kDirectionCount );

  switch( randDirection )
  {
      case kNorth:
        ++state->y;
        break;
        
      case kNE:
        ++state->y;
        ++state->x;
        break;

      case kEast:
        ++state->x;
        break;

      case kSE:
        ++state->x;
        --state->y;
        break;

      case kSouth:
        --state->y;
        break;

      case kSW:
        --state->y;
        --state->x;
        break;
        
      case kWest:
        --state->x;
        break;

      case kNW:
        --state->x;
        ++state->y;
        break;
  }

  // now make sure this dot still fits in the screen (eventually when we draw the dot ourselves we can let it clip)
  if( state->x < 1 )
    state->x = 1;
  if( state->y < 1)
    state->y = 1;

  if( state->x >= kMaxWidth )
    state->x = kMaxWidth - 1;
  if( state->y >= kMaxHeight )
    state->y = kMaxHeight - 1;
}


void cloud( uint8_t* buff )
{
    // this one is more cloud like
    if( s_frame >= kNumDots )
      s_frame = 0;

    draw_pulse( buff, &s_dot[s_frame] );    
    move_dot_randomly( &s_dot[s_frame] );
    ++s_frame;
}


void blob( uint8_t* buff )
{
    // nice and blobby
    for( int i = 0; i < kNumDots; i++ )
    {
      draw_pulse( buff, &s_dot[i] );
      move_dot_randomly( &s_dot[i] );
    } 
}


void disappearing( uint8_t* buff )
{
  for( int i = 0; i < kNumDots; i++ )
  {
    if( s_dot[i].step )
      draw_pulse( buff, &s_dot[i] );
    else
    {
        // find a new position while black
        s_dot[i].x = random( 0, kMaxWidth );
        s_dot[i].y = random( 0, kMaxHeight );
    }
  }
}


void disappearing_accel( uint8_t* buff, float x, float y, float z )
{
  for( int i = 0; i < kNumDots; i++ )
  {
    if( s_dot[i].step )
      draw_pulse( buff, &s_dot[i] );
    else
    {
        // find a new position while black
        move_dot_using_accel( &s_dot[i], x, y, z );
    }
  }
}



void blob_accel( uint8_t* buff, float x, float y, float z )
{
    for( int i = 0; i < kNumDots; i++ )
    {
      draw_pulse( buff, &s_dot[i] );
      if( s_dot[i].step != 0 )
      {
        move_dot_using_accel( &s_dot[i], x, y, z );
      }
      else
      {
        // find a new position while black
        s_dot[i].x = random( 0, kMaxWidth );
        s_dot[i].y = random( 0, kMaxHeight );
      }
    } 
}


// all on (low), test code...
void all_on_low( uint8_t* buff )
{
    memset( buff, 1, sizeof( s_image_buffer ) );
}


#pragma mark -



// Public functions -----------------------------------

void pulsing_dots_setup() 
{
    randomSeed( analogRead( 0 ) );

    for( int i = 0; i < kNumDots; i++ )
    {
#ifdef RANDOM_DURATION
        s_dot[i].num_steps      = random( kMinDotSteps, kNumSteps );
        s_dot[i].step           = random( 0, s_dot[i].num_steps );
#else    
        s_dot[i].num_steps      = kNumSteps;
        s_dot[i].step           = random( 0, kNumSteps );
#endif    
        s_dot[i].x              = random( 0, kMaxWidth );
        s_dot[i].y              = random( 0, kMaxHeight );

        // now make a few dots exceptionally bright
        if( random( 0, 1 ) )
          s_dot[i].max_brightness = kOverBrightness;
        else
          s_dot[i].max_brightness = random( 0, kMaxBrightness );
    }
}


uint8_t* pulsing_dots_get_render_buffer()
{
    return s_buffer_ptr;
}


void pulsing_dots_draw( float x, float y, float z, bool erase ) 
{
    // erase buffer
    if( erase )
        memset( s_buffer_ptr, 0, sizeof( s_image_buffer ) );
    
    // erase to non-black for a test to increase brightness
//    if( erase )
//        memset( s_buffer_ptr, 0xff, sizeof( s_image_buffer ) );

    blob_accel( s_buffer_ptr, x, y, z );

//  disappearing( s_buffer_ptr );
//  disappearing_accel( s_buffer_ptr, y, x, z );
//  cloud( s_buffer_ptr );
//  blob( s_buffer_ptr );
//  all_on_low( s_buffer_ptr );  // for debugging
}

// EOF
