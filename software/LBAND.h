/*
 * Copyright 2022 by Michael Ammann (@mazgch)
 *
 * Licensed under the Apache License, Version 2.0 (the "License");
 * you may not use this file except in compliance with the License.
 * You may obtain a copy of the License at
 *
    http://www.apache.org/licenses/LICENSE-2.0
 *
 * Unless required by applicable law or agreed to in writing, software
 * distributed under the License is distributed on an "AS IS" BASIS,
 * WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
 * See the License for the specific language governing permissions and
 * limitations under the License.
 */
 
#ifndef __LBAND_H__
#define __LBAND_H__

#include "LOG.h"
#include "GNSS.h"

const int LBAND_DETECT_RETRY    = 1000;  //!< Try to detect the received with this intervall
const int LBAND_I2C_ADR         = 0x43;  //!< NEO-D9S I2C address

// helper macros to handle the receiver configuration 
#define LBAND_CHECK_INIT        int _step = 0; bool _ok = true
#define LBAND_CHECK_OK          (_ok)
#define LBAND_CHECK             if (_ok) _step ++, _ok 
#define LBAND_CHECK_EVAL(txt)   if (!_ok) Log.error(txt ", sequence failed at step %d", _step)

class LBAND : public SFE_UBLOX_GNSS {
public:

  LBAND () {
    online = false;
    freq = 0;
    ttagNextTry = millis();
  }

  bool detect() {
    bool ok = begin(UbxWire, LBAND_I2C_ADR );
    if (ok)
    {
      Log.info("LBAND detect receiver detected");
      freq = Config.getFreq();

      ubxVersion("LBAND", this);

/* #*/LBAND_CHECK_INIT;
      setRXMPMPmessageCallbackPtr(onRXMPMPdata);
/* 1*/LBAND_CHECK = setVal16(UBLOX_CFG_PMP_SEARCH_WINDOW,      2200,                    VAL_LAYER_RAM); 
/* 2*/LBAND_CHECK = setVal8(UBLOX_CFG_PMP_USE_SERVICE_ID,      0,                       VAL_LAYER_RAM);           // Default 1 
/* 3*/LBAND_CHECK = setVal16(UBLOX_CFG_PMP_SERVICE_ID,         21845,                   VAL_LAYER_RAM);       // Default 50851
/* 4*/LBAND_CHECK = setVal16(UBLOX_CFG_PMP_DATA_RATE,          2400,                    VAL_LAYER_RAM); 
/* 5*/LBAND_CHECK = setVal8(UBLOX_CFG_PMP_USE_DESCRAMBLER,     1,                       VAL_LAYER_RAM); 
/* 6*/LBAND_CHECK = setVal16(UBLOX_CFG_PMP_DESCRAMBLER_INIT,   26969,                   VAL_LAYER_RAM);       // Default 23560
/* 7*/LBAND_CHECK = setVal8(UBLOX_CFG_PMP_USE_PRESCRAMBLING,   0,                       VAL_LAYER_RAM); 
/* 8*/LBAND_CHECK = setVal64(UBLOX_CFG_PMP_UNIQUE_WORD,        16238547128276412563ull, VAL_LAYER_RAM); 
/* 9*/LBAND_CHECK = setVal32(UBLOX_CFG_PMP_CENTER_FREQUENCY,   freq,                    VAL_LAYER_RAM);
/*10*/LBAND_CHECK = setVal(UBLOX_CFG_MSGOUT_UBX_RXM_PMP_I2C,   1,                       VAL_LAYER_RAM);
/*11*/LBAND_CHECK = setVal(UBLOX_CFG_MSGOUT_UBX_RXM_PMP_UART1, 1,                       VAL_LAYER_RAM); // just in case somebody connects though UART
/*12*/LBAND_CHECK = setVal(UBLOX_CFG_MSGOUT_UBX_RXM_PMP_UART2, 1,                       VAL_LAYER_RAM);
/*13*/LBAND_CHECK = setVal(UBLOX_CFG_MSGOUT_UBX_RXM_PMP_USB,   1,                       VAL_LAYER_RAM); // DEBUG
/*14*/LBAND_CHECK = setVal(UBLOX_CFG_MSGOUT_UBX_MON_PMP_USB,   1,                       VAL_LAYER_RAM); // DEBUG
/*15*/LBAND_CHECK = setVal32(UBLOX_CFG_UART1_BAUDRATE,         38400,                   VAL_LAYER_RAM); // match baudrate with ZED default
/*16*/LBAND_CHECK = setVal32(UBLOX_CFG_UART2_BAUDRATE,         38400,                   VAL_LAYER_RAM); // match baudrate with ZED default
      online = ok = LBAND_CHECK_OK;
      LBAND_CHECK_EVAL("LBAND detect configuration");
      if (ok) {
        Log.info("LBAND detect configuration complete, receiver online");
      }
    }
    return ok;
  }

  void poll(void) {
    HW_DBG_HI(HW_DBG_LBAND);
    if (ttagNextTry <= millis()) {
      ttagNextTry = millis() + LBAND_DETECT_RETRY;
      if (!online) {
        detect();
      }
      
      // do update the ferquency from time to time
      int newFreq = Config.getFreq();
      if (newFreq && (freq != newFreq)) {
        bool ok = true;
        bool changed = false;
        if (online) {
          ok = setVal32(UBLOX_CFG_PMP_CENTER_FREQUENCY, newFreq, VAL_LAYER_RAM);
          if (ok) {
            freq = newFreq;
            changed = true;
            softwareResetGNSSOnly(); // do a restart
          }
          else {
            online = false;
          }
        }
        if (!ok) { 
          Log.error("LBAND updateFreq to %d failed", newFreq);
        } else if (changed) {
          Log.info("LBAND updateFreq to %d", newFreq);
        }
      }
    }
    if (online) {
      checkUblox(); 
      checkCallbacks();
    }
    HW_DBG_LO(HW_DBG_LBAND);
  }
  
protected:
  static void onRXMPMPdata(UBX_RXM_PMP_message_data_t *pmpData)
  {
    size_t wrote = 0;
    if (NULL != pmpData) {
      GNSS::MSG msg;
      uint16_t size = ((uint16_t)pmpData->lengthMSB << 8) | (uint16_t)pmpData->lengthLSB;
      msg.size = size + 8;
      msg.data = new uint8_t[msg.size];
      if (NULL != msg.data) {
        msg.source = GNSS::SOURCE::LBAND;
        memcpy(msg.data, &pmpData->sync1, size + 6);
        memcpy(&msg.data[size + 6], &pmpData->checksumA, 2);
        Log.info("LBAND received RXM-PMP with %d bytes Eb/N0 %.1f dB", msg.size, 0.125 * pmpData->payload[22]);
        Gnss.inject(msg); // Push the sync chars, class, ID, length and payload
      } else {
        Log.error("LBAND received RXM-PMP with %d bytes Eb/N0 %.1f dB, no memory", msg.size, 0.125 * pmpData->payload[22]);
      }
    }
    (void)wrote;
  }

  long freq;
  bool online;
  long ttagNextTry;
};

LBAND LBand;

#endif // __LBAND_H__
