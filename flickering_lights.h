//
//  flickering_lights.h
//  
//
//  Created by Alex Lelievre on 1/6/19.
//

#ifndef flickering_lights_h
#define flickering_lights_h

#include <stdio.h>
#include <Arduino.h>

typedef enum
{
    kFlickering_type_none = 0,
    kFlickering_type_random,
    kFlickering_type_dropout,
    kFlickering_type_flicker_on,
    kFlickering_type_flicker_off,
    kFlickering_type_flicker_mostly_on,
    kFlickering_type_flicker_mostly_off,
    kFlickering_type_flicker_ramp_on,
    kFlickering_type_flicker_ramp_off
} flickering_type;

void     flickering_lights_setup(); 
void     flickering_lights_tick();
void     flash_led( uint8_t num_pulses = 1 );
void     toggle_led();

 
#endif // flickering_lights_h
// EOF
