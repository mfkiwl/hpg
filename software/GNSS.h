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
 
#ifndef __GNSS_H__
#define __GNSS_H__

#include "LOG.h"

const int GNSS_DETECT_RETRY       =  1000;    //!< Try to detect the received with this intervall
const int GNSS_CORRECTION_TIMEOUT = 12000;    //!< If the current correction source has not received data for this period we will switch to the next source that receives data. 
const int GNSS_I2C_ADR            = 0x42;     //!< ZED-F9x I2C address

// helper macro for source handling (selection in the receiver)
#define GNSS_SPARTAN_USESOURCE(source)      ((source == LBAND) ? 1 : 0)
#define GNSS_SPARTAN_USESOURCE_TXT(source)  ((source == LBAND) ? "1-PMP" : "0-SPARTAN")

// helper macros to handle the receiver configuration 
#define GNSS_CHECK_INIT           int _step = 0; bool _ok = true
#define GNSS_CHECK_OK             (_ok)
#define GNSS_CHECK                if (_ok) _step ++, _ok 
#define GNSS_CHECK_EVAL(txt)      if (!_ok) Log.error(txt ", sequence failed at step %d", _step)

// B5 62 27 02 2C 00 02 7F 00 00 00 00 00 00 28 EF 12 05 18 06 1B 14 E8 51 08 24 DD 66 CF AB A8 39 C7 59 67 A4 D7 4E B8 BB 5A 66 2A FF 12 54 FF 32 8D 4A 6E CB
// B5 62 08 05 38 00 02 04 00 00 B5 62 27 02 2C 00 02 7F 00 00 00 00 00 00 28 EF 12 05 18 06 1B 14 E8 51 08 24 DD 66 CF AB A8 39 C7 59 67 A4 D7 4E B8 BB 5A 66 2A FF 12 54 FF 32 8D 4A 6E CB 09 9B

void ubxVersion(const char* tag, SFE_UBLOX_GNSS* rx) {
  struct { char sw[30]; char hw[10]; char ext[10][30]; } info;
  ubxPacket cfg = { UBX_CLASS_MON, UBX_MON_VER, 0, 0, 0, (uint8_t*)&info, 0, 0, SFE_UBLOX_PACKET_VALIDITY_NOT_DEFINED, SFE_UBLOX_PACKET_VALIDITY_NOT_DEFINED};
  rx->setPacketCfgPayloadSize(sizeof(info)+8);
  if (rx->sendCommand(&cfg, 300) == SFE_UBLOX_STATUS_DATA_RECEIVED) {
    char ext[10*32+5] = "";
    char* p = ext;
    for (int i = 0; cfg.len > (30 * i + 40); i ++) {
      if (*info.ext[i]) {
        p += sprintf(p, "%s \"%s\"", (p==ext) ? " ext" : ",", info.ext[i]);
      }
    }
    Log.info("%s version hw %s sw \"%s\"%s", tag, info.hw, info.sw, ext);
  }
} 

extern class GNSS Gnss;
    
class GNSS : public SFE_UBLOX_GNSS {
public:
  
  GNSS() {
    queue = xQueueCreate( 10, sizeof( MSG ) );
    online = false;
    ttagNextTry = millis();
    curSource = OTHER;
    for (int i = 0; i < NUM_SOURCE; i ++) {
      ttagSource[i] = ttagNextTry - GNSS_CORRECTION_TIMEOUT;
    }
  }

  bool detect() {
    bool ok = begin(UbxWire, GNSS_I2C_ADR); //Connect to the Ublox module using Wire port
    if (ok) {
      Log.info("GNSS detect receiver detected");

      ubxVersion("GNSS", this);
    
/* #*/GNSS_CHECK_INIT;
/* 1*/GNSS_CHECK = setI2COutput(COM_TYPE_UBX | COM_TYPE_NMEA );
/* 2*/GNSS_CHECK = setPortInput(COM_PORT_I2C, COM_TYPE_UBX | COM_TYPE_NMEA | COM_TYPE_SPARTN); // Be sure SPARTN input is enabled.
/* 3*/GNSS_CHECK = setNavigationFrequency(1); //Set output in Hz.
/* 4*/GNSS_CHECK = setHighPrecisionMode(true);
      // add some usefull messages to store in the logfile
/* 5*/GNSS_CHECK = setVal(UBLOX_CFG_MSGOUT_UBX_NAV_PVT_I2C,      1, VAL_LAYER_RAM); // required for this app
/* 6*/GNSS_CHECK = setVal(UBLOX_CFG_MSGOUT_UBX_NAV_SAT_I2C,      1, VAL_LAYER_RAM); 
/* 7*/GNSS_CHECK = setVal(UBLOX_CFG_MSGOUT_UBX_NAV_HPPOSLLH_I2C, 1, VAL_LAYER_RAM);
/* 8*/GNSS_CHECK = setVal(UBLOX_CFG_MSGOUT_UBX_NAV_PL_I2C,       1, VAL_LAYER_RAM);
/* 9*/GNSS_CHECK = setVal(UBLOX_CFG_MSGOUT_UBX_RXM_COR_I2C,      1, VAL_LAYER_RAM);
      // Debug to USB port
/*10*/GNSS_CHECK = setVal(UBLOX_CFG_MSGOUT_UBX_NAV_PVT_USB,      1, VAL_LAYER_RAM);
/*11*/GNSS_CHECK = setVal(UBLOX_CFG_MSGOUT_UBX_NAV_SAT_USB,      1, VAL_LAYER_RAM);
/*12*/GNSS_CHECK = setVal(UBLOX_CFG_MSGOUT_UBX_NAV_HPPOSLLH_USB, 1, VAL_LAYER_RAM);
/*13*/GNSS_CHECK = setVal(UBLOX_CFG_MSGOUT_UBX_NAV_PL_USB,       1, VAL_LAYER_RAM);
/*14*/GNSS_CHECK = setVal(UBLOX_CFG_MSGOUT_UBX_RXM_COR_USB,      1, VAL_LAYER_RAM);
/*15*/GNSS_CHECK = setAutoPVTcallbackPtr(onPVTdata);
      online = ok = GNSS_CHECK_OK;
      GNSS_CHECK_EVAL("GNSS detect configuration");
      if (ok) {
        Log.info("GNSS detect configuration complete, receiver online");
        uint8_t key[64];
        int keySize = Config.getValue(CONFIG_VALUE_KEY, key, sizeof(key));
        if (keySize > 0) {
          Log.info("GNSS inject saved keys");
          inject(key, keySize, OTHER);
        }
      }
    }
    return ok;
  }

  typedef enum { WLAN = 0, LTE = 1, LBAND = 2, OTHER = 3, NUM_SOURCE = 3, } SOURCE;
  typedef struct { SOURCE source; uint8_t* data; size_t size; } MSG;
  #define LUT_SRC(s) ((s == WLAN) ? "WLAN" : (s == LTE) ? "LTE" : (s == LBAND) ? "LBAND" : "other") 
  
  size_t inject(MSG& msg) {
    if (xQueueSendToBack(queue, &msg, 0/*portMAX_DELAY*/) == pdPASS) {
      return msg.size;
    }
    delete [] msg.data;
    msg.data = NULL;
    Log.error("GNSS inject %d bytes from %s source failed, queue full", msg.size, LUT_SRC(msg.source));
    return 0;
  }
  
  size_t inject(const uint8_t* ptr, size_t len, SOURCE src) {
    MSG msg;
    msg.data = new uint8_t[len];
    if (NULL != msg.data) {
      memcpy(msg.data, ptr, len);
      msg.size = len;
      msg.source = src;
      return inject(msg);
    }
    Log.error("GNSS inject %d bytes from %s source failed, no memory", msg.size, LUT_SRC(msg.source));
    return 0;
  }
  
  void poll(void) {
    HW_DBG_HI(HW_DBG_GNSS);
    long now = millis();
    if (ttagNextTry<= now) {
      ttagNextTry = now + GNSS_DETECT_RETRY;
      if (!online) {
        detect();
      }
    }
    if (online) {
      checkUblox();
      checkCallbacks();
      if (online) {
        int len = 0;
        MSG msg;
        while (xQueueReceive(queue, &msg, 0/*portMAX_DELAY*/) == pdPASS) {
          if (online) {
            if (msg.source != OTHER) {
              checkSpartanUseSourceCfg(msg.source);
            }
            online = pushRawData(msg.data, msg.size);
            if (online) {
              len += msg.size;
              Log.debug("GNSS inject %d bytes from %s source", msg.size, LUT_SRC(msg.source));
            } else {
              Log.error("GNSS inject %d bytes from %s source failed", msg.size, LUT_SRC(msg.source));
            }
          }
          delete [] msg.data;
          msg.data = NULL;
        }
      }
    }
    HW_DBG_LO(HW_DBG_GNSS);
  }

  #define TIMEOUT_SRC(now, src) ((src < NUM_SOURCE) ? (signed long)((now) - ttagSource[src]) > GNSS_CORRECTION_TIMEOUT : true)

  void checkSpartanUseSourceCfg(SOURCE source) {
    if (source < NUM_SOURCE) {
      // manage correction stream selection 
      long now = millis();
      ttagSource[source] = millis();
      int useSource = GNSS_SPARTAN_USESOURCE(source);
      if ((GNSS_SPARTAN_USESOURCE(curSource) != useSource) && ((curSource == LBAND) || TIMEOUT_SRC(now, curSource))) {
        bool ok/*online*/ = setVal8(UBLOX_CFG_SPARTN_USE_SOURCE, useSource, VAL_LAYER_RAM);
        if (ok) {
          Log.info("GNSS spartanUseSource %s from source %s", GNSS_SPARTAN_USESOURCE_TXT(source), LUT_SRC(source));
          curSource = source;
        } else {
          // WORKAROUND: for some reson the spartanUseSource command fails, reason is unknow, we dont realy worry here and will do it just again next time 
          Log.warning("GNSS spartanUseSource %s from source %s failed", GNSS_SPARTAN_USESOURCE_TXT(source), LUT_SRC(source));
        }
      }
    }
  }
  
  static void onPVTdata(UBX_NAV_PVT_data_t *ubxDataStruct)
  {
    if (ubxDataStruct) {
      const char* fixLut[] = { "No","DR", "2D", "3D", "3D+DR", "TM", "", "" }; 
      const char* carrLut[] = { "No","Float", "Fixed", "" }; 
      uint8_t fixType = ubxDataStruct->fixType; // Print the fix type
      uint8_t carrSoln = ubxDataStruct->flags.bits.carrSoln; // Print the carrier solution
      double fLat = 1e-7 * ubxDataStruct->lat;
      double fLon = 1e-7 * ubxDataStruct->lon;
      Log.info("GNSS %d:%d:%d %02d:%02d:%02d lat %.7f lon %.7f msl %.3f fix %d(%s) carr %d(%s) hacc %.3f source %s heap %d", 
            ubxDataStruct->day, ubxDataStruct->month, ubxDataStruct->year, ubxDataStruct->hour, ubxDataStruct->min,ubxDataStruct->sec, 
            fLat, fLon, 1e-3 * ubxDataStruct->hMSL, fixType, fixLut[fixType & 7], carrSoln, carrLut[carrSoln & 3], 
            1e-3*ubxDataStruct->hAcc, LUT_SRC(Gnss.curSource), ESP.getFreeHeap());
            
      // update the pointperfect topic and lband frequency depending on region we are in
      if (ubxDataStruct->flags.bits.gnssFixOK) {
        Config.updateLocation(fLat, fLon);
      }

#ifdef __WEBSOCKET__H__
      char string[128];
      int len = snprintf(string, sizeof(string), "%02d:%02d:%02d %s %s %s %.3f %.7f %.7f %.3f\r\n",
            ubxDataStruct->hour, ubxDataStruct->min,ubxDataStruct->sec, LUT_SRC(Gnss.curSource), 
            fixLut[fixType & 7], carrLut[carrSoln & 3], 1e-3*ubxDataStruct->hAcc, fLat, fLon, 1e-3 * ubxDataStruct->hMSL);
      Websocket.write(string, len);
#endif
    }
  }

protected:
  
  bool online;
  long ttagNextTry;
  long ttagSource[NUM_SOURCE];
  SOURCE curSource;
  xQueueHandle queue;
};

GNSS Gnss;

// static function that can be easily called from other modules avoiding include dependencies)
size_t GNSSINJECT(const void* ptr, size_t len) { 
  return Gnss.inject((const uint8_t*)ptr, len, GNSS::SOURCE::OTHER); 
}

#endif // __GNSS_H__
