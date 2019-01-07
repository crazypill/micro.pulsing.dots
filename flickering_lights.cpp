//
//  flickering_lights.cpp
//  
//
//  Created by Alex Lelievre on 1/6/19.
//

#include "flickering_lights.h"
#include "arduino_utilities.h"


// Defines -----------------------------------------------------------------


// Constants and static data----------------------------------------------------

static const int16_t kFlashDelayMS           = 100;
static const int16_t kDropoutInitialDelayMS  = 3000;
static const int16_t kDropoutGapMS           = 15;

static bool s_toggle_state = false;


void flicker_random();
void flicker_dropout();
void flicker_on();
void flicker_off();
void flicker_mostly_on();
void flicker_mostly_off();
void flicker_flicker_ramp_on();
void flicker_flicker_ramp_off();



// Code -----------------------------------------------------------------

#pragma mark -

void flickering_lights_setup()
{
    pinMode( LED_BUILTIN, OUTPUT );
//    flash_led( 2 );

    // turn on LED and test different modes that start from on state
//    digitalWrite( LED_BUILTIN, HIGH );
//    flicker_dropout();
}

 
void flickering_lights_tick()
{
//    flicker_dropout();
    flicker_random();
//    delay( kDropoutGapMS );
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
    digitalWrite( LED_BUILTIN, s_toggle_state ? HIGH : LOW ); 
    s_toggle_state = !s_toggle_state;
}


#pragma mark -


void flicker_random()
{
    bool coinflip = coin_flip();
    digitalWrite( LED_BUILTIN, coinflip ? HIGH : LOW );     
}


void flicker_dropout()
{
    // we assume the light is already on... ??? !!@
    delay( kDropoutInitialDelayMS );
    
    for( int i = 0; i < 2; i++ )
    {
        digitalWrite( LED_BUILTIN, LOW );
        delay( kDropoutGapMS );
        digitalWrite( LED_BUILTIN, HIGH );
        delay( kDropoutGapMS );
    }

    digitalWrite( LED_BUILTIN, LOW );
}


void flicker_on()
{
}


void flicker_off()
{
}


void flicker_mostly_on()
{
}


void flicker_mostly_off()
{
}


void flicker_flicker_ramp_on()
{
}


void flicker_flicker_ramp_off()
{
}




// EOF
