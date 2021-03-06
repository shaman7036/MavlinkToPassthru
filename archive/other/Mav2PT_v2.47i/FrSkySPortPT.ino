
// Frsky variables     
short    crc;                         // of frsky-packet
uint8_t  time_slot_max = 16;              
uint32_t time_slot = 1;
float a, az, c, dis, dLat, dLon;
uint8_t sv_count = 0;

#if (Target_Board == 0) // Teensy3x
volatile uint8_t *uartC3;
enum SPortMode { rx , tx };
SPortMode mode, modeNow;

void setSPortMode(SPortMode mode);  // Forward declaration

void setSPortMode(SPortMode mode) {   

  if(mode == tx && modeNow !=tx) {
    *uartC3 |= 0x20;                 // Switch S.Port into send mode
    modeNow=mode;
    #ifdef Frs_Debug_All
    Debug.println("tx");
    #endif
  }
  else if(mode == rx && modeNow != rx) {   
    *uartC3 ^= 0x20;                 // Switch S.Port into receive mode
    modeNow=mode;
    #ifdef Frs_Debug_All
    Debug.println("rx");
    #endif
  }
}
#endif

// ***********************************************************************
void FrSkySPort_Init(void)  {

  for (int i=0 ; i < sb_rows ; i++) {  // initialise sensor table
    sb[i].id = 0;
    sb[i].subid = 0;
    sb[i].millis = 0;
    sb[i].inuse = false;
  }

  
#if (Target_Board == 3) // ESP32
/**  ESP32 only
 * @brief Set UART line inverse mode
 *
 * @param uart_num  UART_NUM_0, UART_NUM_1 or UART_NUM_2
 * @param inverse_mask Choose the wires that need to be inverted.
 *        Inverse_mask should be chosen from 
 *        UART_INVERSE_RXD / UART_INVERSE_TXD / UART_INVERSE_RTS / UART_INVERSE_CTS,
 *        combined with OR operation.
 *
 * @return
 *     - ESP_OK   Success
 *     - ESP_FAIL Parameter error

esp_err_t uart_set_line_inverse(uart_port_t uart_num, uint32_t inverse_mask);

 */
  frSerial.begin(frBaud, SERIAL_8N1, Fr_rxPin, Fr_txPin); 
  
  #ifndef Support_Mavlite && defined Ground_Mode 
    Debug.println("ESP32 S.Port pins inverted for Ground Mode");   
    uart_set_line_inverse(UART_NUM_1, UART_INVERSE_RXD);  // not really needed
    uart_set_line_inverse(UART_NUM_1, UART_INVERSE_TXD);  // line to Taranis or Horus etc
  #else
    Debug.println("ESP32 S.Port pins NOT inverted for Air / Relay Modes / Mavlite. Must use a converter/inverter");  
  #endif  
  
#else

  #if defined Debug_Air_Mode || defined Debug_Relay_Mode || defined Support_Mavlite
    Debug.println("frSerial.begin"); 
  #endif
  
  frSerial.begin(frBaud); // Teensy 3.x, Blue Pill, Maple Mini rx and tx hard wired
#endif

#if (Target_Board == 0) // Teensy3x
 #if (SPort_Serial == 1)
  // Manipulate UART registers for S.Port working
   uartC3   = &UART0_C3;  // UART0 is Serial1
   UART0_C3 = 0x10;       // Invert Serial1 Tx levels
   UART0_C1 = 0xA0;       // Switch Serial1 into single wire mode
   UART0_S2 = 0x10;       // Invert Serial1 Rx levels;
   
 //   UART0_C3 |= 0x20;    // Switch S.Port into send mode
 //   UART0_C3 ^= 0x20;    // Switch S.Port into receive mode
 #else
   uartC3   = &UART2_C3;  // UART2 is Serial3
   UART2_C3 = 0x10;       // Invert Serial1 Tx levels
   UART2_C1 = 0xA0;       // Switch Serial1 into single wire mode
   UART2_S2 = 0x10;       // Invert Serial1 Rx levels;
 #endif
#endif   

} 
// ***********************************************************************

#if defined Air_Mode || defined Relay_Mode || defined Support_Mavlite

void ReadSPort(void) {
  #if defined Debug_Air_Mode || defined Debug_Relay_Mode 
    Debug.println("R SP"); 
  #endif
  uint8_t prevByt=0;
  #if (Target_Board == 0) // Teensy3x
    setSPortMode(rx);
  #endif  
  uint8_t Byt = 0;
  
  while ( frSerial.available())   {  
    Byt =  frSerial.read();

    #if defined Debug_Air_Mode || defined Debug_Relay_Mode || defined Debug_Mavlitexxxxxxxxxxxx
      if (Byt == 0x7E) Debug.println();
      if (Byt == 0x10) Debug.println();
      DisplayByte(Byt);
    #endif
  
    if (prevByt == 0x7E) {  // Start/Stop byte
      sp_start_stop = prevByt;
      
      if (Byt == 0x1B) {    // Sensor_id for injecting S.Port packets
        
        #if defined Debug_Air_Mode || defined Debug_Relay_Mode || defined Debug_Mavlitexxxx
          Debug.println("Start byte & sensor id for injection found "); 
        #endif
        
        #if defined Support_Mavlite
          if (ml20_paramsReq == 2)  {       // mavlite available to inject
            #if defined defined Debug_Mavlite         
              Debug.println("Mavlite_Inject_Packet");
            #endif
            Mavlite_Inject_Packet();
            ml20_paramsReq = 0;            // tri-state  0= none, 1=mavlink reply pending, 2 mav reply received
        }
        #endif
        
        if (FRS_available) {           // passthru available to inject
          Passthru_Inject_Packet();
          FRS_available = false; 
          }

      } 

    #if defined Support_Mavlite      
      if (Byt == 0x0D){  // Sensor ID for Mavlite uplink
        mlBytes[0] =  Byt; 
        for (int i = 1 ; i < 8 ; i++) {
          mlBytes[i] =  frSerial.read(); 
          if ((i>2) && (mlBytes[i-1] == 0x7D) && (mlBytes[i] == 0x5E)) { // correct bytestuff
             mlBytes[i-1] = 0x7E;
             i--;
          }
          if ((i>2) && (mlBytes[i-1] == 0x7D) && (mlBytes[i] == 0x5D)) { // correct bytestuff
             mlBytes[i-1] = 0x7D;
             i--;
          }        
        }    
        ml.sensor_id = mlBytes[0];
        ml.frame_id = mlBytes[1];
        if (ml.frame_id == 0x30) {
          ml.data = *(uint16_t *)(mlBytes+2);
          ml.value = *(uint32_t*)(mlBytes+4);
        
           #if defined Debug_Mavlite 
             Debug.print("S.Port Read raw: ");
             for (int i=0 ; i < 8 ; i++) {
               DisplayByte(mlBytes[i]);
              }
             Debug.printf("\t0x%02X:0x%02X:%04X:%08X\n", ml.sensor_id, ml.frame_id, ml.data, ml.value);          
           #endif   

           if (DecodeMavLite()) {    // Uplink from Taranis/Horus
                     
             ml20_paramsReq = 1;     // 0=none pending, 1=waiting for FC, 2=received_from_fc
             Param_Request_Read(-1, ap_param_id); //   Request Param Read using param_id, not index  //  Param_Request_Read_id(ap_param_id); 
           }
        }      
      }
    #endif
      
    }       
    prevByt=Byt;
  }
  // and back to main loop
}  
#endif

// ***********************************************************************
#if defined Support_Mavlite
bool DecodeMavLite() {
  ml20_seq = ml.raw[2];
  if (ml20_seq == 0) {
    ml20_msg_id = ml.raw[4];
    ml20_paylth = ml.raw[3];
    ml20_idx = 0;
    }
    switch (ml20_msg_id) {
      case 20:    //  #20 or 0x14
        if (Unpack_ml20()) {
          #if defined Debug_Mavlite
            Debug.printf("MavLITE #20 Param_Request_Read :%s:\n", ml20_param_id);  
          #endif  
          strncpy(ap_param_id, ml20_param_id, 16);
          return true;           
        }
        return false;
      case 21:         
        return false;
      case 22:  
        return false;      
    }  
  return false;     
}
#endif
// ***********************************************************************
#if defined Support_Mavlite
void EncodeMavlite()  {    // For  Downlink
  ml22.sensor_id = 0x14;  
  ml22.frame_id = 0x32;
  ml22.seq = 0;
  ml22.paylth = 20;
  ml22.msg_id = 22;
  ml22.value = ap_param_value;
  strncpy(ml22.param_id, ap_param_id, 16);
}
#endif
// ***********************************************************************
#if defined Support_Mavlite
bool Unpack_ml20 () {
  if (ml20_seq == 0) {
    for (int i = 5 ; i < 8 ; i++, ml20_idx++) {
      ml20_param_id[ml20_idx] = ml.raw[i];
      if (ml20_idx >= ml20_paylth) {
        return true;
      }        
    }
    return false;
  }  
        
  if (ml20_seq > 0) {
    for (int i = 3 ; i < 8 ; i++, ml20_idx++) {
      ml20_param_id[ml20_idx] = ml.raw[i];
      if (ml20_idx >= ml20_paylth) {
        ml20_param_id[ml20_idx] = 0x00; // terminate string      
        return true; 
      }                
     }
   return false;
  }
 return false;
}
#endif
// ***********************************************************************

#if not defined Support_Mavlite && defined Ground_Mode
void Emulate_ReadSPort() {
  #if (Target_Board == 0)      // Teensy3x
    setSPortMode(tx);
  #endif
 
  Passthru_Inject_Packet();  

  // and back to main loop
}
#endif

// ***********************************************************************
#if defined Support_Mavlite
void Mavlite_Inject_Packet() {

  EncodeMavlite();  // To Downlink  
  
  uint8_t tot_lth = ml22.paylth + 5;
  #if defined Debug_Mavlite         
    Debug.print("Sending:");
  #endif

  for (int i = 0 ; i < tot_lth ; i++) {
    if (i > 4) {
      FrSkySPort_SendByte(ml22.raw[i], true);  // include for CRC
      #if defined Debug_Mavlite   
        DisplayByte(ml22.raw[i]);
      #endif  
      } else {
      FrSkySPort_SendByte(ml22.raw[i], false); // don't include for CRC
      #if defined Debug_Mavlite   
        DisplayByte(ml22.raw[i]);
      #endif  
      }
  }
  #if defined Debug_Mavlite   
    Debug.printf("CRC=%2X\n", crc);
  #endif  
  FrSkySPort_SendCrc();  //  CRC byte

}
#endif
// ***********************************************************************
void Passthru_Inject_Packet() {

  #if defined Frs_Debug_All || defined Frs_Debug_Period
    ShowPeriod();   
  #endif  
       
  fr_payload = 0; // Clear the payload field
    
  if (!mavGood) return;  // Wait for good Mavlink data

  uint32_t sb_now = millis();
  int16_t sb_age;
  int16_t sb_subid_age;
  int16_t sb_max_tier1 = 0; 
  int16_t sb_max_tier2 = 0; 
  int16_t sb_max       = 0;     
  uint16_t idx_tier1   = 0;                 // row with oldest sensor data
  uint16_t idx_tier2   = 0; 
  uint16_t idx         = 0; 

  // 2 tier scheduling. Tier 1 gets priority, tier2 (0x5000) only sent when tier 1 empty 
  
  // find the row with oldest sensor data = idx 
  sb_unsent = 0;  // how many slots in-use

  uint16_t i = 0;
  while (i < sb_rows) {  
    
    if (sb[i].inuse) {
      sb_unsent++;   
      
      sb_age = (sb_now - sb[i].millis); 
      sb_subid_age = sb_age - sb[i].subid;  

      if (sb[i].id == 0x5000) {
        if (sb_subid_age >= sb_max_tier2) {
          sb_max_tier2 = sb_subid_age;
          idx_tier2 = i;
        }
      } else {
      if (sb_subid_age >= sb_max_tier1) {
        sb_max_tier1 = sb_subid_age;
        idx_tier1 = i;
        }   
      }
    } 
  i++;    
  } 
    
  if (sb_max_tier1 == 0) {            // if there are no tier 1 sensor entries
    if (sb_max_tier2 > 0) {           // but there are tier 2 entries
      idx = idx_tier2;                // send tier 2 instead
      sb_max = sb_max_tier2;
    }
  } else {
    idx = idx_tier1;                  // if there are tier1 entries send them
    sb_max = sb_max_tier1;
  }
  
  //Debug.println(sb_unsent);  // limited detriment :)  
        
  // send the packet if there is one
    if (sb_max > 0) {

     #ifdef Frs_Debug_Scheduler
       Debug.print(sb_unsent); 
       Debug.printf("\tPop  row= %3d", idx );
       Debug.print("  id=");  Debug.print(sb[idx].id, HEX);
       if (sb[idx].id < 0x1000) Debug.print(" ");
       Debug.printf("  subid= %2d", sb[idx].subid);       
       Debug.printf("  payload=%12d", sb[idx].payload );
       Debug.printf("  age=%3d mS \n" , sb_max_tier1 );    
     #endif  
      
    if (sb[idx].id == 0xF101) {
      #ifdef Relay_Mode
        FrSkySPort_SendByte(0x7E, false);   
        FrSkySPort_SendByte(0x1B, false);  
      #endif
     }
                              
    FrSkySPort_SendDataFrame(0x1B, sb[idx].id, sb[idx].payload);
  
    sb[idx].payload = 0;  
    sb[idx].inuse = false; // free the row for re-use
  }
  
 }
// ***********************************************************************     
// ***********************************************************************
void PushToEmptyRow(sb_t pter) {
  
  // find empty sensor row
  uint16_t j = 0;
  while (sb[j].inuse) {
    j++;
  }
  if (j >= sb_rows-1) {
    sens_buf_full_count++;
    if ( (sens_buf_full_count == 0) || (sens_buf_full_count%1000 == 0)) {
      Debug.println("Sensor buffer full. Check S.Port link");  // Report every so often
    }
    return;
  }
  
  sb_unsent++;
  
  #if defined Frs_Debug_Scheduler
    Debug.print(sb_unsent); 
    Debug.printf("\tPush row= %3d", j );
    Debug.print("  id="); Debug.print(pter.id, HEX);
    if (pter.id < 0x1000) Debug.print(" ");
    Debug.printf("  subid= %2d", pter.subid);    
    Debug.printf("  payload=%12d \n", pter.payload );
  #endif

  // The push
  pter.millis = millis();
  pter.inuse = true;
  sb[j] = pter;

}
// ***********************************************************************
void PackSensorTable(uint16_t id, uint8_t subid) {
  
  switch(id) {
    case 0x800:                  // data id 0x800 Lat & Lon
      if (subid == 0) {
        PackLat800(id);
      }
      if (subid == 1) {
        PackLon800(id);
      }
      break; 
           
    case 0x5000:                 // data id 0x5000 Status Text            
        PackMultipleTextChunks_5000(id);
        break;
        
    case 0x5001:                // data id 0x5001 AP Status
      Pack_AP_status_5001(id);
      break; 

    case 0x5002:                // data id 0x5002 GPS Status
      Pack_GPS_status_5002(id);
      break; 
          
    case 0x5003:                //data id 0x5003 Batt 1
      Pack_Bat1_5003(id);
      break; 
                    
    case 0x5004:                // data id 0x5004 Home
      Pack_Home_5004(id);
      break; 

    case 0x5005:                // data id 0x5005 Velocity and yaw
      Pack_VelYaw_5005(id);
      break; 

    case 0x5006:                // data id 0x5006 Attitude and range
      Pack_Atti_5006(id);
      break; 
      
    case 0x5007:                // data id 0x5007 Parameters 
      Pack_Parameters_5007(id);
      break; 
      
    case 0x5008:                // data id 0x5008 Batt 2
      Pack_Bat2_5008(id);
      break; 

    case 0x5009:                // data id 0x5009 Waypoints/Missions 
      Pack_WayPoint_5009(id);
      break;       

    case 0x50F1:                // data id 0x50F1 Servo_Raw            
      Pack_Servo_Raw_50F1(id);
      break;      

    case 0x50F2:                // data id 0x50F2 VFR HUD          
      Pack_VFR_Hud_50F2(id);
      break;    

    case 0x50F3:                // data id 0x50F3 Wind Estimate      
   //   Pack_Wind_Estimate_50F3(id);  // not presently implemented
      break; 
    case 0xF101:                // data id 0xF101 RSSI      
      Pack_Rssi_F101(id);      
      break;       
    default:
      Debug.print("Warning, sensor "); Debug.print(id, HEX); Debug.println(" unknown");
      break;       
  }
              
}
// ***********************************************************************

void FrSkySPort_SendByte(uint8_t byte, bool addCrc) {
  #if (Target_Board == 0)      // Teensy3x
   setSPortMode(tx); 
 #endif  
 if (!addCrc) { 
   frSerial.write(byte);  
   return;       
 }

 CheckByteStuffAndSend(byte);
 
  // update CRC
  crc += byte;       //0-1FF
  crc += crc >> 8;   //0-100
  crc &= 0x00ff;
  crc += crc >> 8;   //0-0FF
  crc &= 0x00ff;
}
// ***********************************************************************
void CheckByteStuffAndSend(uint8_t byte) {
 if (byte == 0x7E) {
   frSerial.write(0x7D);
   frSerial.write(0x5E);
 } else if (byte == 0x7D) {
   frSerial.write(0x7D);
   frSerial.write(0x5D);  
 } else {
   frSerial.write(byte);
   }
}
// ***********************************************************************
void FrSkySPort_SendCrc() {
  uint8_t byte;
  byte = 0xFF-crc;

 CheckByteStuffAndSend(byte);
 
 // DisplayByte(byte);
 // Debug.println("");
  crc = 0;          // CRC reset
}
//***************************************************
void FrSkySPort_SendDataFrame(uint8_t Instance, uint16_t Id, uint32_t value) {

  #if (Target_Board == 0)      // Teensy3x
  setSPortMode(tx); 
  #endif
  
  #if defined Ground_Mode && not defined Support_Mavlite  // Only if ground mode send these bytes, else XSR sends them
    FrSkySPort_SendByte(0x7E, false);       //  START/STOP don't add into crc
    FrSkySPort_SendByte(Instance, false);   //  don't add into crc  
  #endif 
  
  FrSkySPort_SendByte(0x10, true );   //  Data framing byte
 
  uint8_t *bytes = (uint8_t*)&Id;
  #if defined Frs_Debug_Payload
    Debug.print("DataFrame. ID "); 
    DisplayByte(bytes[0]);
    Debug.print(" "); 
    DisplayByte(bytes[1]);
  #endif
  FrSkySPort_SendByte(bytes[0], true);
  FrSkySPort_SendByte(bytes[1], true);
  bytes = (uint8_t*)&value;
  FrSkySPort_SendByte(bytes[0], true);
  FrSkySPort_SendByte(bytes[1], true);
  FrSkySPort_SendByte(bytes[2], true);
  FrSkySPort_SendByte(bytes[3], true);
  
  #if defined Frs_Debug_Payload
    Debug.print("Payload (send order) "); 
    DisplayByte(bytes[0]);
    Debug.print(" "); 
    DisplayByte(bytes[1]);
    Debug.print(" "); 
    DisplayByte(bytes[2]);
    Debug.print(" "); 
    DisplayByte(bytes[3]);  
    Debug.print("Crc= "); 
    DisplayByte(0xFF-crc);
    Debug.println("/");  
  #endif
  
  FrSkySPort_SendCrc();
   
}
//***************************************************
  uint32_t bit32Extract(uint32_t dword,uint8_t displ, uint8_t lth) {
  uint32_t r = (dword & createMask(displ,(displ+lth-1))) >> displ;
  return r;
}
//***************************************************
// Mask then AND the shifted bits, then OR them to the payload
  void bit32Pack(uint32_t dword ,uint8_t displ, uint8_t lth) {   
  uint32_t dw_and_mask =  (dword<<displ) & (createMask(displ, displ+lth-1)); 
  fr_payload |= dw_and_mask; 
}
//***************************************************
  uint32_t bit32Unpack(uint32_t dword,uint8_t displ, uint8_t lth) {
  uint32_t r = (dword & createMask(displ,(displ+lth-1))) >> displ;
  return r;
}
//***************************************************
uint32_t createMask(uint8_t lo, uint8_t hi) {
  uint32_t r = 0;
  for (unsigned i=lo; i<=hi; i++)
       r |= 1 << i;  
  return r;
}
// *****************************************************************

void PackLat800(uint16_t id) {
  fr_gps_status = ap_fixtype < 3 ? ap_fixtype : 3;                   //  0 - 3 
  if (fr_gps_status < 3) return;
  if (px4_flight_stack) {
    fr_lat = Abs(ap_lat24) / 100 * 6;  // ap_lat * 60 / 1000
    if (ap_lat24<0) 
      ms2bits = 1;
    else ms2bits = 0;    
  } else {
    fr_lat = Abs(ap_lat33) / 100 * 6;  // ap_lat * 60 / 1000
    if (ap_lat33<0) 
      ms2bits = 1;
    else ms2bits = 0;
  }
  fr_payload = 0;
  bit32Pack(fr_lat, 0, 30);
  bit32Pack(ms2bits, 30, 2);
          
  #if defined Frs_Debug_All || defined Frs_Debug_LatLon
    Debug.print("Frsky out LatLon 0x800: ");
    Debug.print(" ap_lat33="); Debug.print((float)ap_lat33 / 1E7, 7); 
    Debug.print(" fr_lat="); Debug.print(fr_lat);  
    Debug.print(" fr_payload="); Debug.print(fr_payload); Debug.print(" ");
    DisplayPayload(fr_payload);
    int32_t r_lat = (bit32Unpack(fr_payload,0,30) * 100 / 6);
    Debug.print(" lat unpacked="); Debug.println(r_lat );    
  #endif

  sr.id = id;
  sr.subid = 0;  
  sr.payload = fr_payload;
  PushToEmptyRow(sr);        
}
// *****************************************************************
void PackLon800(uint16_t id) { 
  fr_gps_status = ap_fixtype < 3 ? ap_fixtype : 3;                   //  0 - 3 
  if (fr_gps_status < 3) return;
  if (px4_flight_stack) {
    fr_lon = Abs(ap_lon24) / 100 * 6;  // ap_lon * 60 / 1000
    if (ap_lon24<0) {
      ms2bits = 3;
    }
    else {
      ms2bits = 2;    
    }
  } else {
    fr_lon = Abs(ap_lon33) / 100 * 6;  // ap_lon * 60 / 1000
    if (ap_lon33<0) { 
      ms2bits = 3;
    }
    else {
      ms2bits = 2;
    }
  }
  fr_payload = 0;
  bit32Pack(fr_lon, 0, 30);
  bit32Pack(ms2bits, 30, 2);
          
  #if defined Frs_Debug_All || defined Frs_Debug_LatLon
    Debug.print("Frsky out LatLon 0x800: ");  
    Debug.print(" ap_lon33="); Debug.print((float)ap_lon33 / 1E7, 7);     
    Debug.print(" fr_lon="); Debug.print(fr_lon); 
    Debug.print(" fr_payload="); Debug.print(fr_payload); Debug.print(" ");
    DisplayPayload(fr_payload);
    int32_t r_lon = (bit32Unpack(fr_payload,0,30) * 100 / 6);
    Debug.print(" lon unpacked="); Debug.println(r_lon );  
  #endif

  sr.id = id;
  sr.subid = 1;    
  sr.payload = fr_payload;
  PushToEmptyRow(sr); 
}
// *****************************************************************
void PackMultipleTextChunks_5000(uint16_t id) {

  // status text  char[50] no null,  ap-text char[60]

  for (int i=0; i<50 ; i++) {       // Get text len
    if (ap_text[i]==0) {            // end of text
      len=i;
      break;
    }
  }
  
  ap_text[len+1]=0x00;
  ap_text[len+2]=0x00;  // mark the end of text chunk +
  ap_text[len+3]=0x00;
  ap_text[len+4]=0x00;
          
  ap_txtlth = len;
  
  // look for simple-mode status change messages       
  if (strcmp (ap_text,"SIMPLE mode on") == 0)
    ap_simple = true;
  else if (strcmp (ap_text,"SIMPLE mode off") == 0)
    ap_simple = false;

  fr_severity = ap_severity;
  fr_txtlth = ap_txtlth;
  memcpy(fr_text, ap_text, fr_txtlth+4);   // plus rest of last chunk at least
  fr_simple = ap_simple;

  #if defined Frs_Debug_All || defined Frs_Debug_StatusText
    Debug.print("Frsky out AP_status 0x5000: ");  
    Debug.print(" fr_severity="); Debug.print(fr_severity);
    Debug.print(" "); Debug.print(MavSeverity(fr_severity)); 
    Debug.print(" Text= ");  Debug.print(" |"); Debug.print(fr_text); Debug.println("| ");
  #endif

  fr_chunk_pntr = 0;

  while (fr_chunk_pntr <= (fr_txtlth)) {                 // send multiple 4 byte (32b) chunks
    
    fr_chunk_num = (fr_chunk_pntr / 4) + 1;
    
    fr_chunk[0] = fr_text[fr_chunk_pntr];
    fr_chunk[1] = fr_text[fr_chunk_pntr+1];
    fr_chunk[2] = fr_text[fr_chunk_pntr+2];
    fr_chunk[3] = fr_text[fr_chunk_pntr+3];
    
    fr_payload = 0;
    bit32Pack(fr_chunk[0], 24, 7);
    bit32Pack(fr_chunk[1], 16, 7);
    bit32Pack(fr_chunk[2], 8, 7);    
    bit32Pack(fr_chunk[3], 0, 7);  
    
    #if defined Frs_Debug_All || defined Frs_Debug_StatusText
      Debug.print(" fr_chunk_num="); Debug.print(fr_chunk_num); 
      Debug.print(" fr_txtlth="); Debug.print(fr_txtlth); 
      Debug.print(" fr_chunk_pntr="); Debug.print(fr_chunk_pntr); 
      Debug.print(" "); 
      strncpy(fr_chunk_print,fr_chunk, 4);
      fr_chunk_print[4] = 0x00;
      Debug.print(" |"); Debug.print(fr_chunk_print); Debug.print("| ");
      Debug.print(" fr_payload="); Debug.print(fr_payload); Debug.print(" ");
      DisplayPayload(fr_payload);
      Debug.println();
    #endif  

    if (fr_chunk_pntr+4 > (fr_txtlth)) {

      bit32Pack((fr_severity & 0x1), 7, 1);            // ls bit of severity
      bit32Pack(((fr_severity & 0x2) >> 1), 15, 1);    // mid bit of severity
      bit32Pack(((fr_severity & 0x4) >> 2) , 23, 1);   // ms bit of severity                
      bit32Pack(0, 31, 1);     // filler
      
      #if defined Frs_Debug_All || defined Frs_Debug_StatusText
        Debug.print(" fr_chunk_num="); Debug.print(fr_chunk_num); 
        Debug.print(" fr_severity="); Debug.print(fr_severity);
        Debug.print(" "); Debug.print(MavSeverity(fr_severity)); 
        bool lsb = (fr_severity & 0x1);
        bool sb = (fr_severity & 0x2) >> 1;
        bool msb = (fr_severity & 0x4) >> 2;
        Debug.print(" ls bit="); Debug.print(lsb); 
        Debug.print(" mid bit="); Debug.print(sb); 
        Debug.print(" ms bit="); Debug.print(msb); 
        Debug.print(" fr_payload="); Debug.print(fr_payload); Debug.print(" ");
        DisplayPayload(fr_payload);
        Debug.println(); Debug.println();
     #endif 
     }

    sr.id = id;
    sr.subid = fr_chunk_num;
    sr.payload = fr_payload;
    PushToEmptyRow(sr); 

    #if defined Send_status_Text_3_Times 
      PushToEmptyRow(sr); 
      PushToEmptyRow(sr);
    #endif 
    
    fr_chunk_pntr +=4;
 }
  
  fr_chunk_pntr = 0;
   
}
// *****************************************************************
void DisplayPayload(uint32_t pl)  {
  uint8_t *bytes;

  bytes = (uint8_t*)&pl;
  DisplayByte(bytes[3]);
  Debug.print(" "); 
  DisplayByte(bytes[2]);
  Debug.print(" "); 
  DisplayByte(bytes[1]);
  Debug.print(" "); 
  DisplayByte(bytes[0]);   
}
// *****************************************************************
void Pack_AP_status_5001(uint16_t id) {
  if (ap_type == 6) return;      // If GCS heartbeat ignore it  -  yaapu  - ejs also handled at #0 read
  fr_payload = 0;
  fr_simple = ap_simple;         // Derived from "ALR SIMPLE mode on/off" text messages
  fr_armed = ap_base_mode >> 7;  
  fr_land_complete = fr_armed;
  
  if (px4_flight_stack) 
    fr_flight_mode = PX4FlightModeNum(px4_main_mode, px4_sub_mode);
  else   //  APM Flight Stack
    fr_flight_mode = ap_custom_mode + 1; // AP_CONTROL_MODE_LIMIT - ls 5 bits
  
  bit32Pack(fr_flight_mode, 0, 5);      // Flight mode   0-32 - 5 bits
  bit32Pack(fr_simple ,5, 2);           // Simple/super simple mode flags
  bit32Pack(fr_land_complete ,7, 1);    // Landed flag
  bit32Pack(fr_armed ,8, 1);            // Armed
  bit32Pack(fr_bat_fs ,9, 1);           // Battery failsafe flag
  bit32Pack(fr_ekf_fs ,10, 2);          // EKF failsafe flag
  bit32Pack(px4_flight_stack ,12, 1);   // px4_flight_stack flag

  #if defined Frs_Debug_All || defined Frs_Debug_APStatus
    ShowPeriod(); 
    Debug.print("Frsky out AP_status 0x5001: ");   
    Debug.print(" fr_flight_mode="); Debug.print(fr_flight_mode);
    Debug.print(" fr_simple="); Debug.print(fr_simple);
    Debug.print(" fr_land_complete="); Debug.print(fr_land_complete);
    Debug.print(" fr_armed="); Debug.print(fr_armed);
    Debug.print(" fr_bat_fs="); Debug.print(fr_bat_fs);
    Debug.print(" fr_ekf_fs="); Debug.print(fr_ekf_fs);
    Debug.print(" px4_flight_stack="); Debug.print(px4_flight_stack);
    Debug.print(" fr_payload="); Debug.print(fr_payload); Debug.print(" ");
    DisplayPayload(fr_payload);
    Debug.println();
  #endif

    sr.id = id;
    sr.subid = 0;
    sr.payload = fr_payload;
    PushToEmptyRow(sr);         
}
// *****************************************************************
void Pack_GPS_status_5002(uint16_t id) {
  fr_payload = 0;
  if (ap_sat_visible > 15)
    fr_numsats = 15;
  else
    fr_numsats = ap_sat_visible;
  
  bit32Pack(fr_numsats ,0, 4); 
          
  fr_gps_status = ap_fixtype < 3 ? ap_fixtype : 3;                   //  0 - 3
  fr_gps_adv_status = ap_fixtype > 3 ? ap_fixtype - 3 : 0;           //  4 - 8 -> 0 - 3   
          
  fr_amsl = ap_amsl24 / 100;  // dm
  fr_hdop = ap_eph /10;
          
  bit32Pack(fr_gps_status ,4, 2);       // part a, 3 bits
  bit32Pack(fr_gps_adv_status ,14, 2);  // part b, 3 bits
          
  #if defined Frs_Debug_All || defined Frs_Debug_GPS_status
    ShowPeriod(); 
    Debug.print("Frsky out GPS Status 0x5002: ");   
    Debug.print(" fr_numsats="); Debug.print(fr_numsats);
    Debug.print(" fr_gps_status="); Debug.print(fr_gps_status);
    Debug.print(" fr_gps_adv_status="); Debug.print(fr_gps_adv_status);
    Debug.print(" fr_amsl="); Debug.print(fr_amsl);
    Debug.print(" fr_hdop="); Debug.print(fr_hdop);
  #endif
          
  fr_amsl = prep_number(fr_amsl,2,2);                       // Must include exponent and mantissa    
  fr_hdop = prep_number(fr_hdop,2,1);
          
  #if defined Frs_Debug_All || defined Frs_Debug_GPS_status
    Debug.print(" After prep: fr_amsl="); Debug.print(fr_amsl);
    Debug.print(" fr_hdop="); Debug.print(fr_hdop); 
    Debug.print(" fr_payload="); Debug.print(fr_payload); Debug.print(" ");
    DisplayPayload(fr_payload);
    Debug.println(); 
  #endif     
              
  bit32Pack(fr_hdop ,6, 8);
  bit32Pack(fr_amsl ,22, 9);
  bit32Pack(0, 31,0);  // 1=negative 

  sr.id = id;
  sr.subid = 0;
  sr.payload = fr_payload;
  PushToEmptyRow(sr); 
}
// *****************************************************************  
void Pack_Bat1_5003(uint16_t id) {   //  Into sensor table from #1 SYS_status only
  fr_payload = 0;
  fr_bat1_volts = ap_voltage_battery1 / 100;         // Were mV, now dV  - V * 10
  fr_bat1_amps = ap_current_battery1 ;               // Remain       dA  - A * 10   
  
  // fr_bat1_mAh is populated at #147 depending on battery id.  Into sensor table from #1 SYS_status only.
  //fr_bat1_mAh = Total_mAh1();  // If record type #147 is not sent and good
  
  #if defined Frs_Debug_All || defined Debug_Batteries
    ShowPeriod(); 
    Debug.print("Frsky out Bat1 0x5003: ");   
    Debug.print(" fr_bat1_volts="); Debug.print(fr_bat1_volts);
    Debug.print(" fr_bat1_amps="); Debug.print(fr_bat1_amps);
    Debug.print(" fr_bat1_mAh="); Debug.print(fr_bat1_mAh);
    Debug.print(" fr_payload="); Debug.print(fr_payload); Debug.print(" ");
    DisplayPayload(fr_payload);
    Debug.println();               
  #endif
          
  bit32Pack(fr_bat1_volts ,0, 9);
  fr_bat1_amps = prep_number(roundf(fr_bat1_amps * 0.1F),2,1);          
  bit32Pack(fr_bat1_amps,9, 8);
  bit32Pack(fr_bat1_mAh,17, 15);

  sr.id = id;
  sr.subid = 0;
  sr.payload = fr_payload;
  PushToEmptyRow(sr); 
                       
}
// ***************************************************************** 
void Pack_Home_5004(uint16_t id) {
    fr_payload = 0;
    
    lon1=hom.lon/180*PI;  // degrees to radians
    lat1=hom.lat/180*PI;
    lon2=cur.lon/180*PI;
    lat2=cur.lat/180*PI;

    //Calculate azimuth bearing of craft from home
    a=atan2(sin(lon2-lon1)*cos(lat2), cos(lat1)*sin(lat2)-sin(lat1)*cos(lat2)*cos(lon2-lon1));
    az=a*180/PI;  // radians to degrees
    if (az<0) az=360+az;

    fr_home_angle = Add360(az, -180);                           // Is now the angle from the craft to home in degrees
  
    fr_home_arrow = fr_home_angle * 0.3333;                     // Units of 3 degrees

    // Calculate the distance from home to craft
    dLat = (lat2-lat1);
    dLon = (lon2-lon1);
    a = sin(dLat/2) * sin(dLat/2) + sin(dLon/2) * sin(dLon/2) * cos(lat1) * cos(lat2); 
    c = 2* asin(sqrt(a));    // proportion of Earth's radius
    dis = 6371000 * c;       // radius of the Earth is 6371km

    if (homGood)
      fr_home_dist = (int)dis;
    else
      fr_home_dist = 0;

      fr_home_alt = ap_alt_ag / 100;    // mm->dm
        
   #if defined Frs_Debug_All || defined Frs_Debug_Home
     ShowPeriod(); 
     Debug.print("Frsky out Home 0x5004: ");         
     Debug.print("fr_home_dist=");  Debug.print(fr_home_dist);
     Debug.print(" fr_home_alt=");  Debug.print(fr_home_alt);
     Debug.print(" az=");  Debug.print(az);
     Debug.print(" fr_home_angle="); Debug.print(fr_home_angle);  
     Debug.print(" fr_home_arrow="); Debug.print(fr_home_arrow);         // units of 3 deg  
     Debug.print(" fr_payload="); Debug.print(fr_payload); Debug.print(" ");
     DisplayPayload(fr_payload);
    Debug.println();      
   #endif
   fr_home_dist = prep_number(roundf(fr_home_dist), 3, 2);
   bit32Pack(fr_home_dist ,0, 12);
   fr_home_alt = prep_number(roundf(fr_home_alt), 3, 2);
   bit32Pack(fr_home_alt ,12, 12);
   if (fr_home_alt < 0)
     bit32Pack(1,24, 1);
   else  
     bit32Pack(0,24, 1);
   bit32Pack(fr_home_arrow,25, 7);

   sr.id = id;
   sr.subid = 0;
   sr.payload = fr_payload;
   PushToEmptyRow(sr); 

}

// *****************************************************************
void Pack_VelYaw_5005(uint16_t id) {
  fr_payload = 0;
  
  fr_vy = ap_hud_climb * 10;   // from #74   m/s to dm/s;
  fr_vx = ap_hud_grd_spd * 10;  // from #74  m/s to dm/s

  //fr_yaw = (float)ap_gps_hdg / 10;  // (degrees*100) -> (degrees*10)
  fr_yaw = ap_hud_hdg * 10;              // degrees -> (degrees*10)
  
  #if defined Frs_Debug_All || defined Frs_Debug_YelYaw
    ShowPeriod(); 
    Debug.print("Frsky out VelYaw 0x5005:");  
    Debug.print(" fr_vy=");  Debug.print(fr_vy);       
    Debug.print(" fr_vx=");  Debug.print(fr_vx);
    Debug.print(" fr_yaw="); Debug.print(fr_yaw);
     
  #endif
  if (fr_vy<0)
    bit32Pack(1, 8, 1);
  else
    bit32Pack(0, 8, 1);
  fr_vy = prep_number(roundf(fr_vy), 2, 1);  // Vertical velocity
  bit32Pack(fr_vy, 0, 8);   

  fr_vx = prep_number(roundf(fr_vx), 2, 1);  // Horizontal velocity
  bit32Pack(fr_vx, 9, 8);    
  fr_yaw = fr_yaw * 0.5f;                   // Unit = 0.2 deg
  bit32Pack(fr_yaw ,17, 11);  

 #if defined Frs_Debug_All || defined Frs_Debug_YelYaw
   ShowPeriod(); 
   Debug.print(" After prep:"); \
   Debug.print(" fr_vy=");  Debug.print((int)fr_vy);          
   Debug.print(" fr_vx=");  Debug.print((int)fr_vx);  
   Debug.print(" fr_yaw="); Debug.print((int)fr_yaw);  
   Debug.print(" fr_payload="); Debug.print(fr_payload); Debug.print(" ");
   DisplayPayload(fr_payload);
   Debug.println();                 
 #endif

 sr.id = id;
 sr.subid = 0;
 sr.payload = fr_payload;
 PushToEmptyRow(sr); 
    
}
// *****************************************************************  
void Pack_Atti_5006(uint16_t id) {
  fr_payload = 0;
  
  fr_roll = (ap_roll * 5) + 900;             //  -- fr_roll units = [0,1800] ==> [-180,180]
  fr_pitch = (ap_pitch * 5) + 450;           //  -- fr_pitch units = [0,900] ==> [-90,90]
  fr_range = roundf(ap_range*100);   
  bit32Pack(fr_roll, 0, 11);
  bit32Pack(fr_pitch, 11, 10); 
  bit32Pack(prep_number(fr_range,3,1), 21, 11);
  #if defined Frs_Debug_All || defined Frs_Debug_Attitude
    ShowPeriod(); 
    Debug.print("Frsky out Attitude 0x5006: ");         
    Debug.print("fr_roll=");  Debug.print(fr_roll);
    Debug.print(" fr_pitch=");  Debug.print(fr_pitch);
    Debug.print(" fr_range="); Debug.print(fr_range);
    Debug.print(" Frs_Attitude Payload="); Debug.println(fr_payload);  
  #endif

  sr.id = id;
  sr.subid = 0;
  sr.payload = fr_payload;
  PushToEmptyRow(sr);  
     
}
//***************************************************
void Pack_Parameters_5007(uint16_t id) {

  
  if (paramsID >= 6) {
    fr_paramsSent = true;          // get this done early on and then regularly thereafter
    paramsID = 0;
    return;
  }
  paramsID++;
    
  switch(paramsID) {
    case 1:                                    // Frame type
      fr_param_id = paramsID;
      fr_frame_type = ap_type;
      
      fr_payload = 0;
      bit32Pack(fr_frame_type, 0, 24);
      bit32Pack(fr_param_id, 24, 4);

      #if defined Frs_Debug_All || defined Frs_Debug_Params
        ShowPeriod();  
        Debug.print("Frsky out Params 0x5007: ");   
        Debug.print(" fr_param_id="); Debug.print(fr_param_id);
        Debug.print(" fr_frame_type="); Debug.print(fr_frame_type);  
        Debug.print(" fr_payload="); Debug.print(fr_payload);  Debug.println(" "); 
        DisplayPayload(fr_payload);
        Debug.println();                
      #endif
      
      sr.id = id;     
      sr.subid = 1;
      sr.payload = fr_payload;
      PushToEmptyRow(sr); 

      break;
    case 2:                                   // Previously used to send the battery failsafe voltage
      break;
    case 3:                                   // Previously used to send the battery failsafe capacity in mAh
      break;
    case 4:                                   // Battery pack 1 capacity
      fr_param_id = paramsID;
      #if (Battery_mAh_Source == 2)    // Local
        fr_bat1_capacity = bat1_capacity;
      #elif  (Battery_mAh_Source == 1) //  FC
        fr_bat1_capacity = ap_bat1_capacity;
      #endif 

      fr_payload = 0;
      bit32Pack(fr_bat1_capacity, 0, 24);
      bit32Pack(fr_param_id, 24, 4);

      #if defined Frs_Debug_All || defined Frs_Debug_Params || defined Debug_Batteries
        ShowPeriod();       
        Debug.print("Frsky out Params 0x5007: ");   
        Debug.print(" fr_param_id="); Debug.print(fr_param_id);
        Debug.print(" fr_bat1_capacity="); Debug.print(fr_bat1_capacity);  
        Debug.print(" fr_payload="); Debug.print(fr_payload);  Debug.println(" "); 
        DisplayPayload(fr_payload);
        Debug.println();                   
      #endif
      
      sr.id = id;
      sr.subid = 4;
      sr.payload = fr_payload;
      PushToEmptyRow(sr); 

      break;
    case 5:                                   // Battery pack 2 capacity
      fr_param_id = paramsID;
      #if (Battery_mAh_Source == 2)    // Local
        fr_bat2_capacity = bat2_capacity;
      #elif  (Battery_mAh_Source == 1) //  FC
        fr_bat2_capacity = ap_bat2_capacity;
      #endif  

      fr_payload = 0;
      bit32Pack(fr_bat2_capacity, 0, 24);
      bit32Pack(fr_param_id, 24, 4);
      
      #if defined Frs_Debug_All || defined Frs_Debug_Params || defined Debug_Batteries
        ShowPeriod();  
        Debug.print("Frsky out Params 0x5007: ");   
        Debug.print(" fr_param_id="); Debug.print(fr_param_id);
        Debug.print(" fr_bat2_capacity="); Debug.print(fr_bat2_capacity); 
        Debug.print(" fr_payload="); Debug.print(fr_payload);  Debug.println(" "); 
        DisplayPayload(fr_payload);
        Debug.println();           
      #endif
      
      sr.subid = 5;
      sr.payload = fr_payload;
      PushToEmptyRow(sr); 
       
      break;
    case 6:                                   // Number of waypoints in mission
      fr_param_id = paramsID;
      fr_mission_count = ap_mission_count;

      fr_payload = 0;
      bit32Pack(fr_mission_count, 0, 24);
      bit32Pack(fr_param_id, 24, 4);

      sr.id = id;
      sr.subid = 6;
      PushToEmptyRow(sr);       
      
      #if defined Frs_Debug_All || defined Frs_Debug_Params || defined Debug_Batteries
        ShowPeriod(); 
        Debug.print("Frsky out Params 0x5007: ");   
        Debug.print(" fr_param_id="); Debug.print(fr_param_id);
        Debug.print(" fr_mission_count="); Debug.println(fr_mission_count);           
      #endif
      
      break;    
    }  
}
// ***************************************************************** 
void Pack_Bat2_5008(uint16_t id) {
   fr_payload = 0;
   
   fr_bat2_volts = ap_voltage_battery2 / 100;         // Were mV, now dV  - V * 10
   fr_bat2_amps = ap_current_battery2 ;               // Remain       dA  - A * 10   
   
  // fr_bat2_mAh is populated at #147 depending on battery id
  //fr_bat2_mAh = Total_mAh2();  // If record type #147 is not sent and good
  
  #if defined Frs_Debug_All || defined Debug_Batteries
    ShowPeriod();  
    Debug.print("Frsky out Bat1 0x5003: ");   
    Debug.print(" fr_bat2_volts="); Debug.print(fr_bat2_volts);
    Debug.print(" fr_bat2_amps="); Debug.print(fr_bat2_amps);
    Debug.print(" fr_bat2_mAh="); Debug.print(fr_bat2_mAh);
    Debug.print(" fr_payload="); Debug.print(fr_payload);  Debug.println(" "); 
    DisplayPayload(fr_payload);
    Debug.println();                  
  #endif        
          
  bit32Pack(fr_bat2_volts ,0, 9);
  fr_bat2_amps = prep_number(roundf(fr_bat2_amps * 0.1F),2,1);          
  bit32Pack(fr_bat2_amps,9, 8);
  bit32Pack(fr_bat2_mAh,17, 15);      

  sr.id = id;
  sr.subid = 1;
  sr.payload = fr_payload;
  PushToEmptyRow(sr);          
}


// ***************************************************************** 

void Pack_WayPoint_5009(uint16_t id) {
  fr_payload = 0;
  
  fr_ms_seq = ap_ms_seq;                                      // Current WP seq number, wp[0] = wp1, from regular #42
  
  fr_ms_dist = ap_wp_dist;                                        // Distance to next WP  

  fr_ms_xtrack = ap_xtrack_error;                                 // Cross track error in metres from #62
  fr_ms_target_bearing = ap_target_bearing;                       // Direction of next WP
  fr_ms_cog = ap_cog * 0.01;                                      // COG in degrees from #24
  int32_t angle = (int32_t)wrap_360(fr_ms_target_bearing - fr_ms_cog);
  int32_t arrowStep = 360 / 8; 
  fr_ms_offset = ((angle + (arrowStep/2)) / arrowStep) % 8;       // Next WP bearing offset from COG

  /*
   
0 - up
1 - up-right
2 - right
3 - down-right
4 - down
5 - down - left
6 - left
7 - up - left
 
   */
  #if defined Frs_Debug_All || defined Frs_Debug_Mission
    ShowPeriod();  
    Debug.print("Frsky out RC 0x5009: ");   
    Debug.print(" fr_ms_seq="); Debug.print(fr_ms_seq);
    Debug.print(" fr_ms_dist="); Debug.print(fr_ms_dist);
    Debug.print(" fr_ms_xtrack="); Debug.print(fr_ms_xtrack, 3);
    Debug.print(" fr_ms_target_bearing="); Debug.print(fr_ms_target_bearing, 0);
    Debug.print(" fr_ms_cog="); Debug.print(fr_ms_cog, 0);  
    Debug.print(" fr_ms_offset="); Debug.print(fr_ms_offset);
    Debug.print(" fr_payload="); Debug.print(fr_payload);  Debug.println(" "); 
    DisplayPayload(fr_payload);         
    Debug.println();      
  #endif

  bit32Pack(fr_ms_seq, 0, 10);    //  WP number

  fr_ms_dist = prep_number(roundf(fr_ms_dist), 3, 2);       //  number, digits, power
  bit32Pack(fr_ms_dist, 10, 12);    

  fr_ms_xtrack = prep_number(roundf(fr_ms_xtrack), 1, 1);  
  bit32Pack(fr_ms_xtrack, 22, 6); 

  bit32Pack(fr_ms_offset, 29, 3);  

  sr.id = id;
  sr.subid = 1;
  sr.payload = fr_payload;
  PushToEmptyRow(sr); 
        
}

// *****************************************************************  
void Pack_Servo_Raw_50F1(uint16_t id) {
uint8_t sv_chcnt = 8;
  fr_payload = 0;
  
  if (sv_count+4 > sv_chcnt) { // 4 channels at a time
    sv_count = 0;
    return;
  } 

  uint8_t  chunk = sv_count / 4; 

  fr_sv[1] = PWM_To_63(ap_chan_raw[sv_count]);     // PWM 1000 to 2000 -> 6bit 0 to 63
  fr_sv[2] = PWM_To_63(ap_chan_raw[sv_count+1]);    
  fr_sv[3] = PWM_To_63(ap_chan_raw[sv_count+2]); 
  fr_sv[4] = PWM_To_63(ap_chan_raw[sv_count+3]); 

  bit32Pack(chunk, 0, 4);                // chunk number, 0 = chans 1-4, 1=chans 5-8, 2 = chans 9-12, 3 = chans 13 -16 .....
  bit32Pack(Abs(fr_sv[1]) ,4, 6);        // fragment 1 
  if (fr_sv[1] < 0)
    bit32Pack(1, 10, 1);                 // neg
  else 
    bit32Pack(0, 10, 1);                 // pos          
  bit32Pack(Abs(fr_sv[2]), 11, 6);      // fragment 2 
  if (fr_sv[2] < 0) 
    bit32Pack(1, 17, 1);                 // neg
  else 
    bit32Pack(0, 17, 1);                 // pos   
  bit32Pack(Abs(fr_sv[3]), 18, 6);       // fragment 3
  if (fr_sv[3] < 0)
    bit32Pack(1, 24, 1);                 // neg
  else 
    bit32Pack(0, 24, 1);                 // pos      
  bit32Pack(Abs(fr_sv[4]), 25, 6);       // fragment 4 
  if (fr_sv[4] < 0)
    bit32Pack(1, 31, 1);                 // neg
  else 
    bit32Pack(0, 31, 1);                 // pos  
        
  uint8_t sv_num = sv_count % 4;

  sr.id = id;
  sr.subid = sv_num + 1;
  sr.payload = fr_payload;
  PushToEmptyRow(sr); 

  #if defined Frs_Debug_All || defined Frs_Debug_Servo
    ShowPeriod();  
    Debug.print("Frsky out Servo_Raw 0x5009: ");  
    Debug.print(" sv_chcnt="); Debug.print(sv_chcnt); 
    Debug.print(" sv_count="); Debug.print(sv_count); 
    Debug.print(" chunk="); Debug.print(chunk);
    Debug.print(" fr_sv1="); Debug.print(fr_sv[1]);
    Debug.print(" fr_sv2="); Debug.print(fr_sv[2]);
    Debug.print(" fr_sv3="); Debug.print(fr_sv[3]);   
    Debug.print(" fr_sv4="); Debug.print(fr_sv[4]); 
    Debug.print(" fr_payload="); Debug.print(fr_payload);  Debug.println(" "); 
    DisplayPayload(fr_payload);
    Debug.println();      
          
  #endif

  sv_count += 4; 
}
// *****************************************************************  
void Pack_VFR_Hud_50F2(uint16_t id) {
  fr_payload = 0;
  
  fr_air_spd = ap_hud_air_spd * 10;      // from #74  m/s to dm/s
  fr_throt = ap_hud_throt;               // 0 - 100%
  fr_bar_alt = ap_hud_bar_alt * 10;      // m to dm

  #if defined Frs_Debug_All || defined Frs_Debug_Hud
    ShowPeriod();  
    Debug.print("Frsky out RC 0x50F2: ");   
    Debug.print(" fr_air_spd="); Debug.print(fr_air_spd);
    Debug.print(" fr_throt="); Debug.print(fr_throt);
    Debug.print(" fr_bar_alt="); Debug.print(fr_bar_alt);
    Debug.print(" fr_payload="); Debug.print(fr_payload);  Debug.println(" "); 
    DisplayPayload(fr_payload);
    Debug.println();             
  #endif
  
  fr_air_spd = prep_number(roundf(fr_air_spd), 2, 1);  
  bit32Pack(fr_air_spd, 0, 8);    

  bit32Pack(fr_throt, 8, 7);

  fr_bar_alt =  prep_number(roundf(fr_bar_alt), 3, 2);
  bit32Pack(fr_bar_alt, 15, 12);
  if (fr_bar_alt < 0)
    bit32Pack(1, 27, 1);  
  else
   bit32Pack(0, 27, 1); 
    
  sr.id = id;   
  sr.subid = 1;
  sr.payload = fr_payload;
  PushToEmptyRow(sr); 
        
}
// *****************************************************************  
void Pack_Wind_Estimate_50F3(uint16_t id) {
  fr_payload = 0;
}
// *****************************************************************          
void Pack_Rssi_F101(uint16_t id) {          // data id 0xF101 RSSI tell LUA script in Taranis we are connected
  fr_payload = 0;
  
  if (rssiGood)
    fr_rssi = ap_rssi;            // always %
  else
    fr_rssi = 255;     // We may have a connection but don't yet know how strong. Prevents spurious "Telemetry lost" announcement
  #ifdef RSSI_Override   // dummy rssi override for debugging
    fr_rssi = 70;
  #endif

  bit32Pack(fr_rssi ,0, 32);

  #if defined Frs_Debug_All || defined Debug_Rssi
    ShowPeriod();    
    Debug.print("Frsky out RC 0x5F101: ");   
    Debug.print(" fr_rssi="); Debug.print(fr_rssi);
    Debug.print(" fr_payload="); Debug.print(fr_payload);  Debug.println(" "); 
    DisplayPayload(fr_payload);
    Debug.println();             
  #endif

  sr.id = id;
  sr.subid = 1;
  sr.payload = fr_payload;
  PushToEmptyRow(sr); 
}
//*************************************************** 
int8_t PWM_To_63(uint16_t PWM) {       // PWM 1000 to 2000   ->    nominal -63 to 63
int8_t myint;
  myint = round((PWM - 1500) * 0.126); 
  myint = myint < -63 ? -63 : myint;            
  myint = myint > 63 ? 63 : myint;  
  return myint; 
}

//***************************************************  
uint32_t Abs(int32_t num) {
  if (num<0) 
    return (num ^ 0xffffffff) + 1;
  else
    return num;  
}
//***************************************************  
float Distance(Loc2D loc1, Loc2D loc2) {
float a, c, d, dLat, dLon;  

  loc1.lat=loc1.lat/180*PI;  // degrees to radians
  loc1.lon=loc1.lon/180*PI;
  loc2.lat=loc2.lat/180*PI;
  loc2.lon=loc2.lon/180*PI;
    
  dLat = (loc1.lat-loc2.lat);
  dLon = (loc1.lon-loc2.lon);
  a = sin(dLat/2) * sin(dLat/2) + sin(dLon/2) * sin(dLon/2) * cos(loc2.lat) * cos(loc1.lat); 
  c = 2* asin(sqrt(a));  
  d = 6371000 * c;    
  return d;
}
//*************************************************** 
float Azimuth(Loc2D loc1, Loc2D loc2) {
// Calculate azimuth bearing from loc1 to loc2
float a, az; 

  loc1.lat=loc1.lat/180*PI;  // degrees to radians
  loc1.lon=loc1.lon/180*PI;
  loc2.lat=loc2.lat/180*PI;
  loc2.lon=loc2.lon/180*PI;

  a = sin(dLat/2) * sin(dLat/2) + sin(dLon/2) * sin(dLon/2) * cos(loc2.lat) * cos(loc1.lat); 
  
  az=a*180/PI;  // radians to degrees
  if (az<0) az=360+az;
  return az;
}
//***************************************************
//Add two bearing in degrees and correct for 360 boundary
int16_t Add360(int16_t arg1, int16_t arg2) {  
  int16_t ret = arg1 + arg2;
  if (ret < 0) ret += 360;
  if (ret > 359) ret -= 360;
  return ret; 
}
//***************************************************
// Correct for 360 boundary - yaapu
float wrap_360(int16_t angle)
{
    const float ang_360 = 360.f;
    float res = fmodf(static_cast<float>(angle), ang_360);
    if (res < 0) {
        res += ang_360;
    }
    return res;
}
//***************************************************
// From Arducopter 3.5.5 code
uint16_t prep_number(int32_t number, uint8_t digits, uint8_t power)
{
    uint16_t res = 0;
    uint32_t abs_number = abs(number);

   if ((digits == 1) && (power == 1)) { // number encoded on 5 bits: 4 bits for digits + 1 for 10^power
        if (abs_number < 10) {
            res = abs_number<<1;
        } else if (abs_number < 150) {
            res = ((uint8_t)roundf(abs_number * 0.1f)<<1)|0x1;
        } else { // transmit max possible value (0x0F x 10^1 = 150)
            res = 0x1F;
        }
        if (number < 0) { // if number is negative, add sign bit in front
            res |= 0x1<<5;
        }
    } else if ((digits == 2) && (power == 1)) { // number encoded on 8 bits: 7 bits for digits + 1 for 10^power
        if (abs_number < 100) {
            res = abs_number<<1;
        } else if (abs_number < 1270) {
            res = ((uint8_t)roundf(abs_number * 0.1f)<<1)|0x1;
        } else { // transmit max possible value (0x7F x 10^1 = 1270)
            res = 0xFF;
        }
        if (number < 0) { // if number is negative, add sign bit in front
            res |= 0x1<<8;
        }
    } else if ((digits == 2) && (power == 2)) { // number encoded on 9 bits: 7 bits for digits + 2 for 10^power
        if (abs_number < 100) {
            res = abs_number<<2;
         //   Debug.print("abs_number<100  ="); Debug.print(abs_number); Debug.print(" res="); Debug.print(res);
        } else if (abs_number < 1000) {
            res = ((uint8_t)roundf(abs_number * 0.1f)<<2)|0x1;
         //   Debug.print("abs_number<1000  ="); Debug.print(abs_number); Debug.print(" res="); Debug.print(res);
        } else if (abs_number < 10000) {
            res = ((uint8_t)roundf(abs_number * 0.01f)<<2)|0x2;
          //  Debug.print("abs_number<10000  ="); Debug.print(abs_number); Debug.print(" res="); Debug.print(res);
        } else if (abs_number < 127000) {
            res = ((uint8_t)roundf(abs_number * 0.001f)<<2)|0x3;
        } else { // transmit max possible value (0x7F x 10^3 = 127000)
            res = 0x1FF;
        }
        if (number < 0) { // if number is negative, add sign bit in front
            res |= 0x1<<9;
        }
    } else if ((digits == 3) && (power == 1)) { // number encoded on 11 bits: 10 bits for digits + 1 for 10^power
        if (abs_number < 1000) {
            res = abs_number<<1;
        } else if (abs_number < 10240) {
            res = ((uint16_t)roundf(abs_number * 0.1f)<<1)|0x1;
        } else { // transmit max possible value (0x3FF x 10^1 = 10240)
            res = 0x7FF;
        }
        if (number < 0) { // if number is negative, add sign bit in front
            res |= 0x1<<11;
        }
    } else if ((digits == 3) && (power == 2)) { // number encoded on 12 bits: 10 bits for digits + 2 for 10^power
        if (abs_number < 1000) {
            res = abs_number<<2;
        } else if (abs_number < 10000) {
            res = ((uint16_t)roundf(abs_number * 0.1f)<<2)|0x1;
        } else if (abs_number < 100000) {
            res = ((uint16_t)roundf(abs_number * 0.01f)<<2)|0x2;
        } else if (abs_number < 1024000) {
            res = ((uint16_t)roundf(abs_number * 0.001f)<<2)|0x3;
        } else { // transmit max possible value (0x3FF x 10^3 = 127000)
            res = 0xFFF;
        }
        if (number < 0) { // if number is negative, add sign bit in front
            res |= 0x1<<12;
        }
    }
    return res;
} 
//********************************************************************************
