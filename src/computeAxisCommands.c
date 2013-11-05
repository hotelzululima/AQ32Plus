/*
  October 2012

  aq32Plus Rev -

  Copyright (c) 2012 John Ihlein.  All rights reserved.

  Open Source STM32 Based Multicopter Controller Software

  Includes code and/or ideas from:

  1)AeroQuad
  2)BaseFlight
  3)CH Robotics
  4)MultiWii
  5)S.O.H. Madgwick
  6)UAVX

  Designed to run on the AQ32 Flight Control Board

  This program is free software: you can redistribute it and/or modify
  it under the terms of the GNU General Public License as published by
  the Free Software Foundation, either version 3 of the License, or
  (at your option) any later version.

  This program is distributed in the hope that it will be useful,
  but WITHOUT ANY WARRANTY; without even the implied warranty of
  MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE. See the
  GNU General Public License for more details.

  You should have received a copy of the GNU General Public License
  along with this program. If not, see <http://www.gnu.org/licenses/>.
*/

///////////////////////////////////////////////////////////////////////////////

#include "board.h"
#include "state/harness.h"
#include "state/char_telem.h"


///////////////////////////////////////////////////////////////////////////////

float   attCmd[3];

float   attPID[3];

float   axisPID[4];

float   rateCmd[4];

float   headingReference;
float   heightReference;

uint8_t previousHeadingHoldEngaged = false;
uint8_t previousAltitudeHoldState = false;

///////////////////////////////////////////////////////////////////////////////
// Compute Axis Commands
///////////////////////////////////////////////////////////////////////////////

void computeAxisCommands(float dt)
{
    if (flightMode == ATTITUDE)
    {
        attCmd[ROLL ] = rxCommand[ROLL ] * eepromConfig.attitudeScaling;
        attCmd[PITCH] = rxCommand[PITCH] * eepromConfig.attitudeScaling;
    }

    if (flightMode >= ATTITUDE)
    {
        attPID[ROLL]  = updatePID( attCmd[ROLL ],  sensors.attitude500Hz[ROLL ], dt, holdIntegrators, &eepromConfig.PID[ROLL_ATT_PID ] );
        attPID[PITCH] = updatePID( attCmd[PITCH], -sensors.attitude500Hz[PITCH], dt, holdIntegrators, &eepromConfig.PID[PITCH_ATT_PID] );
    }

    if (flightMode == RATE)
    {
        rateCmd[ROLL ] = rxCommand[ROLL ] * eepromConfig.rateScaling;
        rateCmd[PITCH] = rxCommand[PITCH] * eepromConfig.rateScaling;
    }
    else
    {
        rateCmd[ROLL ] = attPID[ROLL ];
        rateCmd[PITCH] = attPID[PITCH];
    }

    ///////////////////////////////////

    if (headingHoldEngaged == true)  // Heading Hold is ON
    {
        if (previousHeadingHoldEngaged == false)
        {
            setPIDintegralError(HEADING_PID, 0.0f);  // First pass heading hold engaged
            setPIDstates(YAW_RATE_PID,       0.0f);
        }
        rateCmd[YAW] = updatePID( headingReference, heading.mag, dt, holdIntegrators, &eepromConfig.PID[HEADING_PID] );
    }
    else  // Heading Hold is OFF
    {
        rateCmd[YAW] = rxCommand[YAW] * eepromConfig.rateScaling;
        headingReference = heading.mag;
    }

    if (previousHeadingHoldEngaged == true && headingHoldEngaged ==false)
    	{
    	    setPIDintegralError(HEADING_PID, 0.0f);  // First pass heading hold disengaged
    	    setPIDstates(YAW_RATE_PID,       0.0f);
    	}

    previousHeadingHoldEngaged = headingHoldEngaged;

    ///////////////////////////////////

    axisPID[ROLL ] = updatePID( rateCmd[ROLL ],  sensors.gyro500Hz[ROLL ], dt, holdIntegrators, &eepromConfig.PID[ROLL_RATE_PID ] );
    axisPID[PITCH] = updatePID( rateCmd[PITCH], -sensors.gyro500Hz[PITCH], dt, holdIntegrators, &eepromConfig.PID[PITCH_RATE_PID] );
    axisPID[YAW  ] = updatePID( rateCmd[YAW  ],  sensors.gyro500Hz[YAW  ], dt, holdIntegrators, &eepromConfig.PID[YAW_RATE_PID  ] );

    if ( altitudeHoldState == ENGAGED )
      {
      float *hsf = hsf_getState();
      float hDelta = 0.;
      float cDelta = 0.;
      ctIDhhPIDs_t t;
      static int pidTCount = 0;

      if ( previousAltitudeHoldState != ENGAGED )
        {
        setPIDintegralError(H_PID,0.0f);
        setPIDintegralError(HDOT_PID,0.0f);
        setPIDstates(H_PID,0.0f);
        setPIDstates(HDOT_PID,0.0f);
        heightReference = hsf[ hsfZ ];
        cDelta = hDelta = 0.0;
        }

      cDelta = (rxCommand[THROTTLE] - altitudeHoldThrottleValue) * 0.01 ; // Command to meters scaling.

      if (altitudeVelState)
       hDelta += cDelta / 10.0; 
      else
       hDelta = cDelta;

      t.h         = hsf[ hsfZ ];
      t.dh_dt     = hsf[ hsfZdot ];

      t.h_set     = heightReference + hDelta;
      t.dh_dt_set = updatePID( t.h_set,     t.h ,    dt, holdIntegrators, &eepromConfig.PID[ H_PID ] );
      t.throt     = altitudeHoldThrottleValue
                  + updatePID( t.dh_dt_set, t.dh_dt, dt, holdIntegrators, &eepromConfig.PID[ HDOT_PID ] );

      rateCmd[THROTTLE] = t.dh_dt_set;
      axisPID[THROTTLE] = t.throt;

      if ( 0 == pidTCount++ %5 ) 
        ctPushSMTB(ctIDhhPIDS, sizeof(ctIDhhPIDs_t), t.c_ptr );

      }
    else
      axisPID[THROTTLE ] = rxCommand[THROTTLE];

   previousAltitudeHoldState = altitudeHoldState;

}

///////////////////////////////////////////////////////////////////////////////
