//
//  arduino_utilities.h
//  
//
//  Created by Alex Lelievre on 1/6/19.
//

#ifndef arduino_utilities_h
#define arduino_utilities_h

#include <stdio.h>
#include <Arduino.h>


inline void utilities_setup() 
{
    randomSeed( analogRead( 0 ) );
}
 

inline bool coin_flip()
{
    return random( 0, 2 );
}
 
#endif // arduino_utilities_h
// EOF
