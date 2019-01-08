//
//  flickering_lights.cpp
//  
//
//  Created by Alex Lelievre on 1/6/19.
//

#include "flickering_lights.h"
#include "arduino_utilities.h"


// Defines -----------------------------------------------------------------

//#define LED_FLICKER_PIN  LED_BUILTIN
#define LED_FLICKER_PIN  9

typedef struct
{
    int32_t step;
    int32_t start_time;    
} FlickerState;

// this is the flicker function, when it returns true it is done processing and the next function will execute if there's one
typedef bool (*FlickerFunc)( FlickerState* state );


enum
{
    kFlickerDropoutState_Start = 0,
    kFlickerDropoutState_WaitWithLightOn,
    kFlickerDropoutState_FlickerStart,
    kFlickerDropoutState_Flicker,
    kFlickerDropoutState_Dropout,
    kFlickerDropoutState_DropoutDone
};

static const int32_t kFlickerDropoutStateWaitTimeMS = 3000;
static const int32_t kFlickerDropoutFlickerTimeMS   = 1000;
static const int32_t kFlickerDropoutDarkTimeMS      = 5000;


// Constants and static data----------------------------------------------------

static const uint16_t kFlashDelayMS = 100;

static int32_t      s_counter       = 0;
static bool         s_toggle_state  = false;
static FlickerState s_flicker_state = {0};
static FlickerFunc  s_flicker_func  = NULL;

bool flicker_random( FlickerState* state );
bool flicker_dropout( FlickerState* state );
bool flicker_on( FlickerState* state );
bool flicker_off( FlickerState* state );
bool flicker_mostly_on( FlickerState* state );
bool flicker_mostly_off( FlickerState* state );
bool flicker_flicker_ramp_on( FlickerState* state );
bool flicker_flicker_ramp_off( FlickerState* state );

int32_t utime()
{
//    return micros();
    return millis();
}


// Code -----------------------------------------------------------------

#pragma mark -

void flickering_lights_setup()
{
    pinMode( LED_FLICKER_PIN, OUTPUT );

    memset( &s_flicker_state, 0, sizeof( s_flicker_state ) );
    s_flicker_func = flicker_dropout;
}

 
void flickering_lights_tick()
{
    // change this code to get next function from a fairly dynamic stack -  !!@

    // just keep calling the routine over and over until it is done
    if( s_flicker_func && s_flicker_func( &s_flicker_state ) )
    {
        if( s_flicker_func == flicker_dropout )
        {
            ++s_counter;
            if( s_counter > 5 ) // drop out 5 times
            {
                s_counter = 0;
                s_flicker_func = flicker_flicker_ramp_on;
            }
        }
        else if( s_flicker_func == flicker_flicker_ramp_on )
        {
            ++s_counter;
            if( s_counter > 2 ) // ramp twice
            {
                s_counter = 0;
                s_flicker_func = flicker_random;
            }
        }
        else
        {
            ++s_counter;
            if( s_counter > 10000 )   // random routine only takes one cycle so
                s_flicker_func = NULL;                     
        }
            
        memset( &s_flicker_state, 0, sizeof( s_flicker_state ) );   // clear this for next func
    }
}


void flash_led( uint8_t num_pulses )
{
  for( int i = 0; i < num_pulses; i++ )
  {
    digitalWrite( LED_BUILTIN, HIGH );
    delay( kFlashDelayMS );
    digitalWrite( LED_BUILTIN, LOW );
    delay( kFlashDelayMS );
  }
}


void toggle_led()
{
    digitalWrite( LED_FLICKER_PIN, s_toggle_state ? HIGH : LOW ); 
    s_toggle_state = !s_toggle_state;
}


#pragma mark -


bool flicker_random( FlickerState* state )
{
    digitalWrite( LED_FLICKER_PIN, coin_flip() ? HIGH : LOW );     
    return true;
}


bool flicker_dropout( FlickerState* state )
{
    int32_t current  = utime();
    int32_t interval = current - state->start_time;
    
    // first step is to wait a long time with the light on, so we take the starting time
    if( state->step == kFlickerDropoutState_Start )
    {
        // turn on the light
        digitalWrite( LED_FLICKER_PIN, HIGH );
        state->start_time = current;   // take the time...
        state->step++;  // go to next step
        return false;   // we aren't done yet
    }
        
    // next step is to look at the time, stay here till we cross the threshold
    if( state->step == kFlickerDropoutState_WaitWithLightOn )
    {
        if( interval >= kFlickerDropoutStateWaitTimeMS )
            state->step++;
        return false;
    }
    

    if( state->step == kFlickerDropoutState_FlickerStart )
    {
        state->start_time = current;   // take the time...
        state->step++;
        return false;
    }
    
    if( state->step == kFlickerDropoutState_Flicker )
    {
        digitalWrite( LED_FLICKER_PIN, coin_flip() ? HIGH : LOW );
        if( interval >= kFlickerDropoutFlickerTimeMS )
            state->step++;
        return false;
    }

    if( state->step == kFlickerDropoutState_Dropout )
    {
        digitalWrite( LED_FLICKER_PIN, LOW );
        state->start_time = current;   // take the time...
        state->step++;
        return false;
    }

    if( state->step == kFlickerDropoutState_DropoutDone )
        return interval > kFlickerDropoutDarkTimeMS;

    return true;
}


bool flicker_on( FlickerState* state )
{
    return true;
}


bool flicker_off( FlickerState* state )
{
    return true;
}


bool flicker_mostly_on( FlickerState* state )
{
    return true;
}


bool flicker_mostly_off( FlickerState* state )
{
    return true;
}


bool flicker_flicker_ramp_on( FlickerState* state )
{
    analogWrite( LED_FLICKER_PIN, state->step );
    state->step += 5;
    if( state->step > 255 )
    {
        state->step = 0;
        return true;
    }
        
    return false;
}


bool flicker_flicker_ramp_off( FlickerState* state )
{
    return true;
}




// EOF
