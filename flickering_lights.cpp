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
#define STACK_MAX        5 // keep this low for testing...

typedef struct
{
    uint32_t step;
    uint32_t start_time;  
    uint32_t param;  
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
    kFlickerDropoutState_DropoutBlipStart,
    kFlickerDropoutState_DropoutBlip,
    kFlickerDropoutState_DropoutDone
};

enum
{
     kFlickerDropoutStateWaitTimeMS   = 3000,
     kFlickerDropoutFlickerTimeMS     = 1000,
     kFlickerDropoutDarkTimeMS        = 5000,
     kFlickerDropoutBlipMinTimeMS     = 1200,      // this is the time we wait till blip (random between min and max)
     kFlickerDropoutBlipMaxTimeMS     = 3000,
     kFlickerDropoutBlipMinDurationMS = 50,
     kFlickerDropoutBlipMaxDurationMS = 90,
     
     kFlickerBrownoutMinIntensity = 10,
     kFlickerBrownoutMaxIntensity = 80,

     kFlickerFlourescentMinIntensity = 230,
     kFlickerFlourescentMaxIntensity = 255
};

// this is the function stack
static FlickerFunc  s_func_stack[STACK_MAX] = { NULL };
static FlickerFunc* s_func_stack_base       = &s_func_stack[0];  
static FlickerFunc* s_func_stack_top        = &s_func_stack[STACK_MAX];  
static FlickerFunc* s_func_stack_pointer    = s_func_stack_base;  


// Constants and static data----------------------------------------------------

static const uint16_t kFlashDelayMS = 100;

static int32_t      s_counter       = 0;
static bool         s_toggle_state  = false;
static FlickerState s_flicker_state = {0};
static FlickerFunc  s_flicker_func  = NULL;

bool flicker_random( FlickerState* state );
bool flicker_dropout( FlickerState* state );
bool flicker_brownout( FlickerState* state );
bool flicker_on( FlickerState* state );
bool flicker_off( FlickerState* state );
bool flicker_mostly_on( FlickerState* state );
bool flicker_mostly_off( FlickerState* state );
bool flicker_flicker_ramp_on( FlickerState* state );
bool flicker_flicker_ramp_off( FlickerState* state );


// Code -----------------------------------------------------------------

#pragma mark -

void flickering_lights_setup()
{
    pinMode( LED_FLICKER_PIN, OUTPUT );

    memset( &s_flicker_state, 0, sizeof( s_flicker_state ) );
    s_flicker_func = flicker_dropout; // flicker_brownout
}

 
void flickering_lights_tick()
{
    // change this code to get next function from a fairly dynamic stack -  !!@

    // just keep calling the routine over and over until it is done
    if( s_flicker_func && s_flicker_func( &s_flicker_state ) )
    {
        if( s_flicker_func == flicker_dropout ) //flicker_brownout
        {
            ++s_counter;
            if( s_counter > 1 )
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

#pragma mark -

// State stack -----------------------------------------------------------------




#pragma mark -


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
    if( coin_flip() )
        analogWrite( LED_FLICKER_PIN, random( 0, 256 ) );   // flicker at different brightnesses like a real broken bulb    
    else
        digitalWrite( LED_FLICKER_PIN, LOW );
             
    return true;
}


bool flicker_dropout( FlickerState* state )
{
    uint32_t current  = millis();
    uint32_t interval = current - state->start_time;    
    
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
        state->start_time = current;
        state->step++;
        return false;
    }
    
    if( state->step == kFlickerDropoutState_Flicker )
    {
        flicker_random( NULL );
        if( interval >= kFlickerDropoutFlickerTimeMS )
            state->step++;
        return false;
    }

    if( state->step == kFlickerDropoutState_Dropout )
    {
        digitalWrite( LED_FLICKER_PIN, LOW );
        state->start_time = current;    
        state->param      = random( kFlickerDropoutBlipMinTimeMS, kFlickerDropoutBlipMaxTimeMS );    // when to blip (blip might not even happen as its value is also random)
        state->step++;
        return false;
    }

    // wait with the light off
    if( state->step == kFlickerDropoutState_DropoutBlipStart )
    {
        if( interval >= state->param )  // random wait time here
        {
            state->start_time = current;
            state->param = random( kFlickerDropoutBlipMinDurationMS, kFlickerDropoutBlipMaxDurationMS );
            state->step++;
        }
        return false;
    }

    if( state->step == kFlickerDropoutState_DropoutBlip )
    {
        flicker_random( NULL );
        if( interval >= state->param )  // random wait time again
        {
            state->start_time = current;
            state->param = 0;
            state->step++;
        }
        return false;
    }

    if( state->step == kFlickerDropoutState_DropoutDone )
    {
        digitalWrite( LED_FLICKER_PIN, LOW );
        return interval > kFlickerDropoutDarkTimeMS;
    }

    return true;
}


bool flicker_brownout( FlickerState* state )
{
    uint32_t current  = millis();
    uint32_t interval = current - state->start_time;    
    
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
        state->start_time = current;
        state->step++;
        return false;
    }
    
    if( state->step == kFlickerDropoutState_Flicker )
    {
        flicker_random( NULL );
        if( interval >= kFlickerDropoutFlickerTimeMS )
            state->step++;
        return false;
    }

    if( state->step == kFlickerDropoutState_Dropout )
    {
        analogWrite( LED_FLICKER_PIN, random( kFlickerBrownoutMinIntensity, kFlickerBrownoutMaxIntensity ) );
        state->start_time = current;    
        state->param      = random( kFlickerDropoutBlipMinTimeMS, kFlickerDropoutBlipMaxTimeMS );    // when to blip (blip might not even happen as its value is also random)
        state->step++;
        return false;
    }

    // wait with the light off
    if( state->step == kFlickerDropoutState_DropoutBlipStart )
    {
        if( interval >= state->param )  // random wait time here
        {
            state->start_time = current;
            state->param = random( kFlickerDropoutBlipMinDurationMS, kFlickerDropoutBlipMaxDurationMS );
            state->step++;
        }
        return false;
    }

    if( state->step == kFlickerDropoutState_DropoutBlip )
    {
        flicker_random( NULL );
        if( interval >= state->param )  // random wait time again
        {
            state->start_time = current;
            state->param = 0;
            state->step++;
        }
        return false;
    }

    if( state->step == kFlickerDropoutState_DropoutDone )
    {
        analogWrite( LED_FLICKER_PIN, random( kFlickerBrownoutMinIntensity, kFlickerBrownoutMaxIntensity ) );
        return interval > kFlickerDropoutDarkTimeMS;
    }

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
    
    // brown-out flicker first
    if( state->step > 220 )
        analogWrite( LED_FLICKER_PIN, random( kFlickerBrownoutMinIntensity, kFlickerBrownoutMaxIntensity ) );

    if( state->step > 255 )
    {
        state->step = 0;
        return true;
    }
        
    return false;
}


bool flicker_flicker_ramp_off( FlickerState* state )
{
//    if( state->step > 220 )
//        analogWrite( LED_FLICKER_PIN, random( kFlickerFlourescentMinIntensity, kFlickerFlourescentMaxIntensity ) );
    return true;
}




// EOF
