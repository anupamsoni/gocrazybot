//-----------------------------------------------------------------------------
// F02x_PCA0_Software_Timer_Blinky.c
//-----------------------------------------------------------------------------
// Copyright 2006 Silicon Laboratories, Inc.
// http://www.silabs.com
//
// Program Description:
//
// This program uses the PCA's Software Timer Mode to periodically schedule
// an interrupt.  Inside the ISR, a port pin is toggled to blink an LED on the
// target board.
//
// In this example, PCA Module 0 is used to generate the interrupt, and the
// port pin driving the LED is configured for push-pull mode.
//
// Pinout:
//
//   P1.6 - LED (push-pull)
//
//   all other port pins unused
//
//
// How To Test:
//
// 1) Download code to a 'F02x target board with the LED jumper ON.
// 2) Run the program - the LED should blink at 5 Hz.
//
// FID:            02X000019
// Target:         C8051F02x
// Tool chain:     Keil C51 8.0 / Keil EVAL C51
// Command Line:   None
//
//
// Release 1.0
//    -Initial Revision (BD / TP)
//    -3 AUG 2006
//

//-----------------------------------------------------------------------------
// Includes
//-----------------------------------------------------------------------------

#include <C8051F020.h>                 // SFR declarations

//-----------------------------------------------------------------------------
// Global Constants
//-----------------------------------------------------------------------------

#define SYSCLK       22118400          // External oscillator frequency in Hz

#define LED_FREQUENCY 22000                // Frequency to blink LED at in Hz
#define T0_CLOCKS 221                  // Use 221 clocks per T0 Overflow

// SYSCLK cycles per interrupt
#define PCA_TIMEOUT ((SYSCLK/T0_CLOCKS)/LED_FREQUENCY/2)

sbit LEFT = P1^5;                       // LED='1' means ON
sbit RIGHT = P1^6;                       // LED='1' means ON
sbit SW1 = P3^7;                   // SW1 ='0' means switch pressed

//-----------------------------------------------------------------------------
// Function Prototypes
//-----------------------------------------------------------------------------

void Oscillator_Init (void);
void Port_Init (void);
void PCA0_Init (void);

//-----------------------------------------------------------------------------
// Global Variables
//-----------------------------------------------------------------------------

unsigned int NEXT_COMPARE_VALUE;       // Next edge to be sent out in HSO mode
 int PWMONE =0;	
 int PWMTWO =0;      

//-----------------------------------------------------------------------------
// main() Routine
//-----------------------------------------------------------------------------

void main (void) {

	
   // Disable watchdog timer
   WDTCN = 0xde;
   WDTCN = 0xad;

   Port_Init ();                       // Initialize crossbar and GPIO
   Oscillator_Init ();                 // Initialize oscillator
   PCA0_Init ();                        // Initialize PCA0

   EA = 1;                             // Globally enable interrupts

	
   while (1);                          // Spin here to wait for ISR
   {
      
   }                                   // end of while(1)	
}


//-----------------------------------------------------------------------------
// Initialization Subroutines
//-----------------------------------------------------------------------------

//-----------------------------------------------------------------------------
// Oscillator_Init
//-----------------------------------------------------------------------------
//
// Return Value : None
// Parameters   : None
//
// This function initializes the system clock to use the external oscillator
// at 22.1184 MHz.
//
//-----------------------------------------------------------------------------
void Oscillator_Init (void)
{
   unsigned int i;                     // Delay counter

   OSCXCN = 0x67;                      // Start external oscillator with
                                       // 22.1184 MHz crystal

   for (i = 0; i < 256; i++) ;         // Wait for oscillator to start

   while (!(OSCXCN & 0x80)) ;          // Wait for crystal osc. to settle

   OSCICN = 0x88;                      // Select external oscillator as SYSCLK
                                       // source and enable missing clock
                                       // detector
}

//-----------------------------------------------------------------------------
// Port_Init
//-----------------------------------------------------------------------------
//
// Return Value : None
// Parameters   : None
//
// This function configures the crossbar and GPIO ports.
//
// No crossbar peripherals are used for this example.
//
// P1.6 is set to Push-Pull mode.
//
//-----------------------------------------------------------------------------
void Port_Init (void)
{
   XBR0 = 0x00;
   XBR1 = 0x00;
   XBR2 = 0x40;                        // No peripherals routed to pins
                                       // Enable crossbar and weak pull-ups

   P1MDOUT |= 0x40;                    // Set LED (P1.6) to push-pull
   P1MDOUT |= 0x20;                    // Set LED (P1.5) to push-pull
   P3MDOUT = 0x00;                     // P3.7 is open-drain

   P3     |= 0x80;                     // Set P3.7 latch to '1'
}

//-----------------------------------------------------------------------------
// PCA0_Init
//-----------------------------------------------------------------------------
//
// Return Value : None
// Parameters   : None
//
// This function configures the PCA time base to use T0 Overflows as a clock
// source.  Timer0 is also configured by this routine for 8-bit auto-reload
// mode, where T0_CLOCKS is the number of System clocks per T0 Overflow.
// The code then sets up Software Timer mode for Module 0 (CEX0 pin).
//
// On every interrupt from Module 0, software toggles the port I/O pin for the
// LED. The frequency of the LED toggling is determined by the parameter
// CEX0_FREQUENCY.
//
// The maximum frequency output for this example is about 50 kHz.
// The minimum frequency output for this example is about 1 Hz
//
// The PCA time base in this example is configured to use T0 overflows, and
// SYSCLK is set up for the external oscillator running at 22.1184 MHz.
//
// Using different PCA clock sources, different T0 reload values, or a
// different processor clock will result in a different frequency for the LED
// square wave, and different maximum and minimum options.
//
//    -------------------------------------------------------------------------
//    How "Software Timer Mode" Works:
//
//       The PCA's Software Timer Mode works by generating an interrupt for the
//    associated module every time the PCA0 register increments and the new
//    16-bit PCA0 counter value matches the module's capture/compare
//    register (PCA0CPn).
//
//    By loading the PCA0CPn register with the next match value every time a
//    match happens, arbitrarily timed interrupts can be generated.
//    -------------------------------------------------------------------------
//
// When setting the capture/compare register for the next match value, the low
// byte of the PCA0CPn register (PCA0CPLn) should be written first, followed
// by the high byte (PCA0CPHn).  Writing the low byte clears the ECOMn bit,
// and writing the high byte will restore it.  This ensures that a match does
// not occur until the full 16-bit value has been written to the compare
// register.  Writing the high byte first will result in the ECOMn bit being
// set to '0' after the 16-bit write, and the next match will not occur at
// the correct time.
//
// It is best to update the capture/compare register as soon after a match
// occurs as possible so that the PCA counter will not have incremented past
// the next desired edge value. This code implements the compare register
// update in the PCA ISR upon a match interrupt.
//
//-----------------------------------------------------------------------------
void PCA0_Init (void)
{
   unsigned short PCA_value = 0;

   // Configure Timer 0 for 8-bit auto-reload mode, using SYSCLK as time base
   TMOD &= 0xF0;                       // Clear all T0 control bits
   TMOD |= 0x02;                       // 8-bit auto-reload timer
   CKCON |= 0x08;                      // T0 uses SYSCLK
   TH0 = -T0_CLOCKS;                   // Set up reload value
   TL0 = -T0_CLOCKS;                   // Set up initial value

   // Configure PCA time base; overflow interrupt disabled
   PCA0CN = 0x00;                      // Stop counter; clear all flags
   PCA0MD = 0x04;                      // Use Timer 0 as time base

   PCA0CPM0 = 0x49;                    // Module 0 = Software Timer Mode,
                                       // Enable Module 0 Interrupt flag,
                                       // Enable ECOM bit

   PCA0L = 0x00;                       // Reset PCA Counter Value to 0x0000
   PCA0H = 0x00;

   PCA0CPL0 = PCA_TIMEOUT & 0x00FF;    // Set up first match
   PCA0CPH0 = (PCA_TIMEOUT & 0xFF00) >> 8;

   // Set up the variable for the following match
   PCA_value = PCA0CPL0;
   PCA_value |= PCA0CPH0 << 8;

   NEXT_COMPARE_VALUE = PCA_value + PCA_TIMEOUT;

   EIE1 |= 0x08;                       // Enable PCA interrupts

   CR = 1;                             // Start PCA

   TR0 = 1;                            // Start Timer 0
}

//-----------------------------------------------------------------------------
// Interrupt Service Routines
//-----------------------------------------------------------------------------

//-----------------------------------------------------------------------------
// PCA0_ISR
//-----------------------------------------------------------------------------
//
// Return Value : None
// Parameters   : None
//
// This is the ISR for the PCA.  It handles the case when a match occurs on
// channel 0, and updates the PCA0CPn compare register with the value held in
// the global variable "NEXT_COMPARE_VALUE".
//
//-----------------------------------------------------------------------------
void PCA0_ISR (void) interrupt 9
{
   unsigned short PCA_value = 0;
   

   if (CCF0)                           // If Module 0 caused the interrupt
   {
      CCF0 = 0;                        // Clear module 0 interrupt flag.

      PCA0CPL0 = (NEXT_COMPARE_VALUE & 0x00FF);
      PCA0CPH0 = (NEXT_COMPARE_VALUE & 0xFF00)>>8;

      if (SW1 == 0)                    // If switch depressed
      { 
       
	  RIGHT = ~RIGHT;                      // Invert the pin status
	  LEFT = ~LEFT;                      // Invert the pin status
      }
	  else   
      {  
		LEFT = 0;                     // Else, turn it off
		RIGHT  = 0;
	  }
	  
	  	  
      // Set up the variable for the following edge
      PCA_value = PCA0CPL0;
      PCA_value |= PCA0CPH0 << 8;

      NEXT_COMPARE_VALUE = PCA_value + PCA_TIMEOUT;
   }
   else                                // Interrupt was caused by other bits.
   {
      PCA0CN &= ~0x86;                 // Clear other interrupt flags for PCA
   }
}


//-----------------------------------------------------------------------------
// End Of File
//-----------------------------------------------------------------------------