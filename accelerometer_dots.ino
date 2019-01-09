//--------------------------------------------------------------------------
//
//  pulsing_accelerometer_dots.ino
//
//  Created by Alex Lelievre on 12/11/18. 
//     This is based on the FirePendant project from https://learn.adafruit.com/animated-flame-pendant/software
//
//   - Feather M0 Basic proto or Pro Trinket microcontroller (adafruit.com/product/2010 or 2000)
//     (#2010 = 3V/12MHz for longest battery life, but 5V/16MHz works OK)
//   - Charlieplex LED Matrix Driver (2946)
//   - Charlieplex LED Matrix (2947, 2948, 2972, 2973 or 2974)
//   - 350 mAh LiPoly battery (2750)
//   - LiPoly backpack (2124)
//   - SPDT Slide Switch (805)
//--------------------------------------------------------------------------

// Most power stuff removed for M0 using ARDUINO_SAMD_ZERO define

#include <Wire.h>            // For I2C communication
#include <Adafruit_LIS3DH.h>
#include <Adafruit_Sensor.h>

#ifndef ARDUINO_SAMD_ZERO
#include <avr/power.h>     // Peripheral control and
#include <avr/sleep.h>     // sleep to minimize current draw
#endif

#include "flickering_lights.h"
#include "pulsing_dots.h"
#include "arduino_utilities.h"


// Defines -----------------------------------------------------------------

Adafruit_LIS3DH lis = Adafruit_LIS3DH();

#if defined( ARDUINO_SAMD_ZERO ) && defined( SERIAL_PORT_USBVIRTUAL )
  // Required for Serial on Zero based boards
  #define Serial SERIAL_PORT_USBVIRTUAL
#endif

// NOTE: that I could not get the Adafruit LED Matrix Driver to work on address 0x75
#define DISPLAY1 0x74        // I2C address of Charlieplex matrix
#define DISPLAY2 0x77        // I2C address of Charlieplex matrix

#define Z_IS_UP   // our dev board has z up but in production the boards are standing up(sidedown)
#define USE_ACCELEROMETER
#define RENDER_DOTS

#ifndef ARDUINO_SAMD_ZERO
// turn this define on for power savings on boards that support it
//#define POWER_SAVINGS // disable for serial debugging too
#endif


// Constants -----------------------------------------------------------------

static const bool     kShouldErase    = true;

static uint8_t        s_display_one_page = 0;     // Front/back buffer control
#ifdef TWO_DISPLAYS
static uint8_t        s_display_two_page = 0;     // Front/back buffer control for second display
#endif


#pragma mark -


// UTILITY FUNCTIONS -------------------------------------------------------

// The full IS31FL3731 library is NOT used by this code.  Instead, 'raw'
// writes are made to the matrix driver.  This is to maximize the space
// available for animation data.  Use the Adafruit_IS31FL3731 and
// Adafruit_GFX libraries if you need to do actual graphics stuff.

// Begin I2C transmission and write register address (data then follows)
void writeRegister( uint8_t address, uint8_t n ) 
{
  Wire.beginTransmission( address );
  Wire.write( n );
  // Transmission is left open for additional writes
}

// Select one of eight IS31FL3731 pages, or Function Registers
void pageSelect( uint8_t address, uint8_t n ) 
{
  writeRegister( address, 0xFD ); // Command Register
  Wire.write( n );       // Page number (or 0xB = Function Registers)
  Wire.endTransmission();
}


void setup_display_controller( uint8_t address ) 
{
  uint8_t i;
  pageSelect( address, 0x0B );             // Access the Function Registers
  writeRegister( address, 0 );             // Starting from first...

  // Clear all except Shutdown
  for( i = 0; i < 13; i++ ) 
    Wire.write(10 == i); 
  Wire.endTransmission();
  
  for( int p = 0; p < 2; p++ ) 
  {                                          // For each page used (0 & 1)...
    pageSelect( address, p );                // Access the Frame Registers
    writeRegister( address, 0 );             // Start from 1st LED control reg
    for( i = 0; i < 18; i++ ) 
      Wire.write( 0xFF );                    // Enable all LEDs (18*8=144)
    
    for( int byteCounter = i + 1; i < 0xB4; i++ ) 
    {                                        // For blink & PWM registers...
      Wire.write( 0 );                       // Clear all
      if( ++byteCounter >= 32 ) 
      {                                      // Every 32 bytes...
        byteCounter = 1;                     // End I2C transmission and
        Wire.endTransmission();              // start a new one because
        writeRegister( address, i );         // Wire buf is only 32 bytes.
      }
    }
    Wire.endTransmission();
  }
}


void buffer_frame( uint8_t address, const uint8_t* buff, uint8_t* page ) 
{
  uint8_t  a, x1, y1, x2, y2, x, y;

  // Display frame rendered on prior pass.  This is done at function start
  // (rather than after rendering) to ensire more uniform animation timing.
  pageSelect( address, 0x0B );    // Function registers
  writeRegister( address, 0x01 ); // Picture Display reg
  Wire.write( *page );            // Page #
  Wire.endTransmission();

  *page ^= 1; // Flip front/back buffer index

  // Write buff to matrix (not actually displayed until next pass)
  pageSelect( address, *page );    // Select background buffer
  writeRegister( address, 0x24 ); // First byte of PWM data
  uint8_t i = 0, byteCounter = 1;
  for( uint8_t x = 0; x < kDeviceHeight; x++ ) 
  {
    for( uint8_t y = 0; y < kDeviceWidth; y++ ) 
    {
      Wire.write( buff[i++] );    // Write each byte to matrix
      if( ++byteCounter >= 32 ) 
      {                                     // Every 32 bytes...
        Wire.endTransmission();             // end transmission and
        writeRegister( address, 0x24 + i ); // start a new one (Wire lib limits)
      }
    }
  }
  Wire.endTransmission();
}


#pragma mark -



// SETUP FUNCTION - RUNS ONCE AT STARTUP -----------------------------------

void setup() 
{
    utilities_setup();
    flickering_lights_setup();
    pulsing_dots_setup();
    
#ifdef POWER_SAVINGS
  power_all_disable(); // Stop peripherals: ADC, timers, etc. to save power
  power_twi_enable();  // But switch I2C back on; need it for display
  DIDR0 = 0x0F;        // Digital input disable on A0-A3
#endif // POWER_SAVINGS   

  Wire.begin();                            // Initialize I2C
#ifdef ARDUINO_SAMD_ZERO
  Wire.setClock( 400000 );                 // 400 kHz I2C - Feather M0
#else  
  // The TWSR/TWBR lines are AVR-specific and won't work on other MCUs.
  TWSR = 0;                                // I2C prescaler = 1
  TWBR = (F_CPU / 400000 - 16) / 2;        // 400 KHz I2C
#endif  // ARDUINO_SAMD_ZERO

  // setup the LED controllers
  setup_display_controller( DISPLAY1 ); 
#ifdef TWO_DISPLAYS  
  setup_display_controller( DISPLAY2 );
#endif

#ifdef USE_ACCELEROMETER
  if( !lis.begin( 0x18 ) ) 
    Serial.println( "Couldnt start accelerometer" );
  else
    lis.setRange( LIS3DH_RANGE_4_G );   // 2, 4, 8 or 16 G!
#endif   // USE_ACCELEROMETER

#ifdef POWER_SAVINGS
  // Enable the watchdog timer, set to a ~32 ms interval (about 31 Hz)
  // This provides a sufficiently steady time reference for animation,
  // allows timer/counter peripherals to remain off (for power saving)
  // and can power-down the chip after processing each frame.
  set_sleep_mode(SLEEP_MODE_PWR_DOWN); // Deepest sleep mode (WDT wakes)
  noInterrupts();
  MCUSR  &= ~_BV(WDRF);
  WDTCSR  =  _BV(WDCE) | _BV(WDE);     // WDT change enable
  WDTCSR  =  _BV(WDIE) | _BV(WDP0);    // Interrupt enable, ~32 ms
  interrupts();
  // Peripheral and sleep savings only amount to about 10 mA, but this
  // may provide nearly an extra hour of run time before battery depletes.
#endif  // POWER_SAVINGS
}


#pragma mark -

// LOOP FUNCTION - RUNS EVERY FRAME ----------------------------------------

void loop() 
{
#ifdef POWER_SAVINGS
  power_twi_enable();
#endif

  flickering_lights_tick();

#ifdef USE_ACCELEROMETER
  float           accel_scale = 0.5f;
  sensors_event_t event       = {0}; 
  lis.getEvent( &event );  
//  Serial.print( "x: " ); Serial.println( event.acceleration.x );
#endif  // USE_ACCELEROMETER

#ifdef RENDER_DOTS
//    uint32_t start_time = millis();

    // render a frame - about 19ms on Pro Trinket 12Mhz
#ifdef USE_ACCELEROMETER
  #ifdef Z_IS_UP
    // display is laying down = Z is up
    pulsing_dots_draw( event.acceleration.y * accel_scale, event.acceleration.x * accel_scale, event.acceleration.z * accel_scale, kShouldErase );
  #else
    // display is standing vertically - Y is up
    pulsing_dots_draw( event.acceleration.z * accel_scale, -event.acceleration.y * accel_scale, event.acceleration.x * accel_scale, kShouldErase );
  #endif // Z_IS_UP
#else
    pulsing_dots_draw( 0, 0, 0, kShouldErase );
#endif  // USE_ACCELEROMETER

//    uint32_t render_time = millis() - start_time;
//    Serial.print( "render_time (ms): " ); Serial.println( render_time );

    // output the frame
    uint8_t* buf = pulsing_dots_get_render_buffer();
    buffer_frame( DISPLAY1, buf, &s_display_one_page );
#ifdef TWO_DISPLAYS
    buffer_frame( DISPLAY2, &buf[144], &s_display_two_page );
#endif
    
    // Total render time about 60ms on Pro Trinket 12Mhz (so 40ms spent talking over i2c)
//    uint32_t elapsed_time = millis() - start_time;
//    Serial.print( "elapsed_time (ms): " ); Serial.println( elapsed_time );
#else
    // Total render time about 60ms on Pro Trinket 12Mhz
    delay( 60 );
#endif // RENDER_DOTS

  if( kFrameDelayMS )
    delay( kFrameDelayMS );
 
#ifdef POWER_SAVINGS
  power_twi_disable(); // I2C off (see comment at top of function)
  sleep_enable();
  interrupts();
  sleep_mode();        // Power-down MCU.
  // Code will resume here on wake; loop() returns and is called again
#endif // POWER_SAVINGS
}


// Interrupt service routines ----------------------------------------

#ifdef POWER_SAVINGS
ISR( WDT_vect ) 
{ 
    // Watchdog timer interrupt (does nothing, but required)
} 
#endif

// EOF






