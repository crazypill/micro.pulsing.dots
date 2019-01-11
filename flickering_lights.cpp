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
#define STACK_MAX        50

typedef struct
{
    uint32_t step;
    uint32_t start_time;  
    uint32_t param;  
    uint32_t counter;
} FlickerState;

// this is the flicker function, when it returns true it is done processing and the next function will execute if there's one
typedef bool (*FlickerFunc)( FlickerState* state );

// ------------------------------------------
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
    kFlickerDropoutStateWaitTimeMS   = 6000,
    kFlickerDropoutFlickerTimeMS     = 1000,
    kFlickerDropoutDarkTimeMS        = 4000,
    kFlickerDropoutBlipMinTimeMS     = 1200,      // this is the time we wait till blip (random between min and max)
    kFlickerDropoutBlipMaxTimeMS     = 2500,
    kFlickerDropoutBlipMinDurationMS = 50,
    kFlickerDropoutBlipMaxDurationMS = 90,

    kFlickerBrownoutMinIntensity     = 40,
    kFlickerBrownoutMaxIntensity     = 100,

    kFlickerBurstMinIntensity        = 230,
    kFlickerBurstMaxIntensity        = 255,

    kFlickerFlourescentMaxIntensity  = 220
};


// ------------------------------------------
enum
{
    kFlickerState_Start = 0,
    kFlickerState_Wait,
    kFlickerState_FlickerStart,
    kFlickerState_Flicker
};

enum
{
    kFlickerMostlyOffMinTimeMS  = 100,      // this is the time we wait till flicker (random between min and max)
    kFlickerMostlyOffMaxTimeMS  = 1000,
    
    kFlickerMostlyOnMinTimeMS   = 20,      // this is the time we wait till flicker (random between min and max)
    kFlickerMostlyOnMaxTimeMS   = 400,
    
    kFlickerMostlyMinDurationMS = 50,
    kFlickerMostlyMaxDurationMS = 90,
    
    kFlickerMostlyMinIntensity  = 200,
    kFlickerMostlyMaxIntensity  = 220,

    kFlickerMostlyFlickerCount  = 10
};


// Forward declares ------------------------------------------------------

bool flicker_random( FlickerState* state );
bool flicker_dropout( FlickerState* state );
bool flicker_brownout( FlickerState* state );
bool flicker_on( FlickerState* state );
bool flicker_off( FlickerState* state );
bool flicker_mostly_on( FlickerState* state );
bool flicker_mostly_off( FlickerState* state );
bool flicker_ramp_on( FlickerState* state );
bool flicker_ramp_off( FlickerState* state );
bool flicker_bad_wiring( FlickerState* state );

void randomly_fill_stack();

void        stack_push( FlickerFunc );
FlickerFunc stack_pop();
int32_t     stack_depth();


// Constants and static data ---------------------------------------------

static const uint16_t kFlashDelayMS = 100;

static int32_t      s_counter       = 0;
static bool         s_toggle_state  = false;
static FlickerState s_flicker_state = {0};
static FlickerFunc  s_flicker_func  = NULL;

// this is the function stack
static FlickerFunc  s_func_stack[STACK_MAX] = { NULL };
static int32_t      s_func_stack_index      = 0;  

// we use this to randomize the array
static FlickerFunc  s_func_array[] = 
{ 
    flicker_random,
    flicker_dropout,
    flicker_brownout,
    flicker_brownout,
    flicker_on,         // we stack a bunch of these to increase the chances of the light staying on more often than flickering
    flicker_on,
    flicker_on,
    flicker_on,
    flicker_on,
    flicker_on,
    flicker_off,
    flicker_off,
    flicker_off,
    flicker_mostly_on,
    flicker_mostly_off,
    flicker_ramp_on,
    flicker_ramp_on,
    flicker_ramp_on,
    flicker_ramp_off,
    flicker_bad_wiring,
    flicker_bad_wiring
};

#define countof( a ) (sizeof( a ) / sizeof( a[0] ))


// Code -----------------------------------------------------------------

#pragma mark -

void flickering_lights_setup()
{
    pinMode( LED_FLICKER_PIN, OUTPUT );

    memset( &s_flicker_state, 0, sizeof( s_flicker_state ) );
    randomly_fill_stack();
}

 
void flickering_lights_tick()
{
    if( !s_flicker_func )
        randomly_fill_stack();
        
    // just keep calling the routine over and over until it is done
    if( s_flicker_func && s_flicker_func( &s_flicker_state ) )
    {
        s_flicker_func = stack_pop();
        memset( &s_flicker_state, 0, sizeof( s_flicker_state ) );   // clear this for next func
    }
}

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
        // turn on the light but not all the way!
        analogWrite( LED_FLICKER_PIN, kFlickerFlourescentMaxIntensity );
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
        analogWrite( LED_FLICKER_PIN, kFlickerFlourescentMaxIntensity );
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



// these all ramp up like a real flourescent light !!@ - implement me!!@  
bool flicker_on( FlickerState* state )
{
    // this should blink a number of times, then go super bright, then dim to "normal"
    analogWrite( LED_FLICKER_PIN, random( kFlickerBurstMinIntensity, kFlickerBurstMaxIntensity ) );
    return true;
}


// these all ramp up like a real flourescent light !!@ - implement me!!@ 
bool flicker_off( FlickerState* state )
{
    // this should just be a series of blinks, then off
    analogWrite( LED_FLICKER_PIN, random( kFlickerBurstMinIntensity, kFlickerBurstMaxIntensity ) );
    return true;
}


bool flicker_mostly_on( FlickerState* state )
{
    uint32_t current  = millis();
    uint32_t interval = current - state->start_time;    

    // take a random amount of time to wait
    if( state->step == kFlickerState_Start )
    {
        state->start_time = current;    
        state->param      = random( kFlickerMostlyOnMinTimeMS, kFlickerMostlyOnMaxTimeMS );    // how long to wait till a flicker
        state->step++;
        return false;
    }

    if( state->step == kFlickerState_Wait )
    {
        if( interval >= state->param )
            state->step++;
        return false;
    }

    if( state->step == kFlickerState_FlickerStart )
    {
        state->start_time = current;
        state->param      = random( kFlickerMostlyMinDurationMS, kFlickerMostlyMaxDurationMS ); // how long is this flicker
        state->step++;
        return false;
    }
    
    if( state->step == kFlickerState_Flicker )
    {
        // we want bright flickers, flicker_random produces alot of low flickers too which we don't want, it's all or nothing here
        analogWrite( LED_FLICKER_PIN, random( kFlickerMostlyMinIntensity, kFlickerMostlyMaxIntensity ) );
        if( interval >= state->param )
        {
            digitalWrite( LED_FLICKER_PIN, LOW );
            state->step = 0; // restart the cycle a few times
        }
        
        return (++state->counter > kFlickerMostlyFlickerCount);
    }

    return true;
}


// !!@ refactor this- it's exactly the same as mostly_on but with different constants !!@
bool flicker_mostly_off( FlickerState* state )
{
    uint32_t current  = millis();
    uint32_t interval = current - state->start_time;    

    // take a random amount of time to wait
    if( state->step == kFlickerState_Start )
    {
        state->start_time = current;    
        state->param      = random( kFlickerMostlyOffMinTimeMS, kFlickerMostlyOffMaxTimeMS );    // how long to wait till a flicker
        state->step++;
        return false;
    }

    if( state->step == kFlickerState_Wait )
    {
        if( interval >= state->param )
            state->step++;
        return false;
    }

    if( state->step == kFlickerState_FlickerStart )
    {
        state->start_time = current;
        state->param      = random( kFlickerMostlyMinDurationMS, kFlickerMostlyMaxDurationMS ); // how long is this flicker
        state->step++;
        return false;
    }
    
    if( state->step == kFlickerState_Flicker )
    {
        // we want bright flickers, flicker_random produces alot of low flickers too which we don't want, it's all or nothing here
        analogWrite( LED_FLICKER_PIN, random( kFlickerMostlyMinIntensity, kFlickerMostlyMaxIntensity ) );
        if( interval >= state->param )
        {
            digitalWrite( LED_FLICKER_PIN, LOW );
            state->step = 0; // restart the cycle a few times
        }
        
        return (++state->counter > kFlickerMostlyFlickerCount);
    }

    return true;
}



bool flicker_ramp_on( FlickerState* state )
{
    analogWrite( LED_FLICKER_PIN, state->step );
    state->step += 5;
    
    // brown-out flicker
    if( state->step > 220 )
        analogWrite( LED_FLICKER_PIN, random( kFlickerBrownoutMinIntensity, kFlickerBrownoutMaxIntensity ) );

    if( state->step > 255 )
    {
        state->step = 0;
        return true;
    }
        
    return false;
}


bool flicker_ramp_off( FlickerState* state )
{
    analogWrite( LED_FLICKER_PIN, 255 - state->step );
    state->step += 5;
    
    // blast brightness right at end
    if( state->step > 220 )
        analogWrite( LED_FLICKER_PIN, random( kFlickerBurstMinIntensity, kFlickerBurstMaxIntensity ) );

    if( state->step > 255 )
    {
        state->step = 0;
        return true;
    }
        
    return false;
}


bool flicker_bad_wiring( FlickerState* state )
{
    uint32_t current  = millis();
    uint32_t interval = current - state->start_time;    

    // randomly wiggle the amplitude
    analogWrite( LED_FLICKER_PIN, random( kFlickerBrownoutMinIntensity, kFlickerBrownoutMaxIntensity ) );

    // take a random amount of time to wait
    if( state->step == kFlickerState_Start )
    {
        state->start_time = current;    
        state->param      = random( kFlickerMostlyOnMinTimeMS, kFlickerDropoutDarkTimeMS );    // how long wiggle the amplitude
        state->step++;
        return false;
    }

    return interval >= state->param;
}



#pragma mark -

void randomly_fill_stack()
{
    // use array of all funcs, and randomly choose from it to fill the stack
    for( int i = 0; i < STACK_MAX; i++ )
    {
        int8_t index = random( 0, countof( s_func_array ) );
        stack_push( s_func_array[index] );
    }
    
    s_flicker_func = stack_pop();    
}



#pragma mark -

// State stack -----------------------------------------------------------------

void stack_push( FlickerFunc func )
{
    if( s_func_stack_index > STACK_MAX )
    {
        s_func_stack_index = STACK_MAX - 1;
        Serial.println( "flicker stack overflow!" );
        return;
    }
    s_func_stack[s_func_stack_index++] = func;
}


FlickerFunc stack_pop()
{
    FlickerFunc func = s_func_stack[--s_func_stack_index];
    if( s_func_stack_index < 0 )
    {
        s_func_stack_index = 0;
        return NULL;
    }
    return func;
}


int32_t stack_depth()
{
    return s_func_stack_index;
}

// EOF
