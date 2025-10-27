#include "LoRaWan_APP.h"
#include "Aurora.h"

// =================================================================
//  Configuration for Aurora Inverter (RS485)
// =================================================================
#define INVERTER_ADDRESS 2
#define UNUSED_PIN 4       

// Initialize the Aurora inverter communication object.
// Use Serial2 (GPIO 17-TX, 18-RX) for RS485 communication.
Aurora inverter(INVERTER_ADDRESS, &Serial2, UNUSED_PIN);

// =================================================================
//  LoRaWAN Configuration (Your keys and settings)
// =================================================================

/* OTAA parameters */
uint8_t devEui[] = { 0x6F, 0x48, 0x64, 0xB4, 0xB6, 0xB7, 0x15, 0xBE };
uint8_t appEui[] = { 0x33, 0x9C, 0xC4, 0xC0, 0x60, 0x6B, 0x3C, 0x73 };
uint8_t appKey[] = { 0x32, 0x13, 0xA6, 0xEF, 0x0B, 0xAB, 0xAF, 0x67, 0xB2, 0x32, 0x99, 0x74, 0x83, 0x62, 0xA9, 0xF3 };

/* ABP parameters (not used) */
uint8_t nwkSKey[] = { 0x15, 0xb1, 0xd0, 0xef, 0xa4, 0x63, 0xdf, 0xbe, 0x3d, 0x11, 0x18, 0x1e, 0x1e, 0xc7, 0xda,0x85 };
uint8_t appSKey[] = { 0xd7, 0x2c, 0x78, 0x75, 0x8c, 0xdc, 0xca, 0xbf, 0x55, 0xee, 0x4a, 0x77, 0x8d, 0x16, 0xef,0x67 };
uint32_t devAddr = (uint32_t)0x007e6ae1;

/* LoRaWAN Channels Mask, Region, and Class */
uint16_t userChannelsMask[6] = { 0x00FF, 0x0000, 0x0000, 0x0000, 0x0000, 0x0000 };
LoRaMacRegion_t loraWanRegion = LORAMAC_REGION_AS923;
DeviceClass_t loraWanClass = CLASS_A;

/* Set transmission interval to 30 seconds for testing */
uint32_t appTxDutyCycle = 30000;

/* 修改1: 启用OTAA激活方式 */
bool overTheAirActivation = true; // 启用OTAA激活方式以适应实地测试
/* 修改2: 启用ADR */
bool loraWanAdr = true; // 启用ADR以适应网络条件变化
bool isTxConfirmed = false; // Use unconfirmed for regular telemetry
uint8_t confirmedNbTrials = 8;

// Define the single FPort we will use for all data
#define UNIFIED_DATA_FPORT 10

// =================================================================
//  Global Variables and Data Structures
// =================================================================
uint8_t appPort = UNIFIED_DATA_FPORT;

struct InverterData {
    float gridPower;
    float gridVoltage;
    float pvVoltage;
    float inverterTemperature;
    uint32_t dailyEnergy;
    //uint32_t totalUptime;
    uint8_t globalState;
    uint8_t alarmState;
    uint8_t lastAlarms[4];
    char partNumber[7]; // 6 chars + null terminator
    char serialNumber[7]; 
};
InverterData inverterData;

// =================================================================
//  Data Acquisition and Payload Preparation
// =================================================================

bool readAllInverterData() {
    bool success = true;

    // Read Dynamic Data
    Aurora::DataDSP dsp;
    dsp = inverter.readDSP(3, 0);
    if (dsp.state.readState) { inverterData.gridPower = dsp.value; } else { success = false; Serial.println("[Failed] to read Grid Power"); }
    
    dsp = inverter.readDSP(1, 0); 
    if (dsp.state.readState) { inverterData.gridVoltage = dsp.value; } else { success = false; Serial.println("[Failed] to read Grid Voltage"); }
    
    dsp = inverter.readDSP(23, 0); 
    if (dsp.state.readState) { inverterData.pvVoltage = dsp.value; } else { success = false; Serial.println("[Failed] to read PV Voltage"); }
    
    dsp = inverter.readDSP(21, 0);
    if (dsp.state.readState) { inverterData.inverterTemperature = dsp.value; } else { success = false; Serial.println("[Failed] to read Inverter Temp"); }

    // Read Aggregate Data
    Aurora::DataCumulatedEnergy energy = inverter.readCumulatedEnergy(0);
    if (energy.state.readState) { inverterData.dailyEnergy = energy.energy; } else { success = false; Serial.println("[Failed] to read Daily Energy"); }
    
    /*Can't read total uptime, temporarily commented out */
    //Aurora::DataTimeCounter time = inverter.readTimeCounter(0);
    //if (time.state.readState) { inverterData.totalUptime = time.upTimeInSec; } else { success = false; 
    //    Serial.println("Failed to read Uptime");
    //}
    
    // Read Status Data
    Aurora::DataState state = inverter.readState();
    if (state.state.readState) {
        inverterData.globalState = state.state.globalState;
        inverterData.alarmState = state.alarmState;
    } else { success = false; Serial.println("[Failed] to read State"); }
    
    Aurora::DataLastFourAlarms alarms = inverter.readLastFourAlarms();
    if (alarms.state.readState) { memcpy(inverterData.lastAlarms, &alarms.alarm1, 4); } else { success = false; Serial.println("[Failed] to read Alarms"); }

    // Read Static Data
    Aurora::DataSystemPN pn = inverter.readSystemPN();
    if(pn.readState) { strncpy(inverterData.partNumber, pn.PN.c_str(), 6); } else { success = false; Serial.println("[Failed] to read Part Number"); }
    
    Aurora::DataSystemSerialNumber sn = inverter.readSystemSerialNumber();
    if(sn.readState) { strncpy(inverterData.serialNumber, sn.SerialNumber.c_str(), 6); } else { success = false; Serial.println("[Failed] to read Serial Number"); }

    return success;
}

static void prepareTxFrame() {
    appPort = UNIFIED_DATA_FPORT;
    memset(appData, 0, sizeof(appData));
    appDataSize = 38;
    int offset = 0;

    Serial.println("\n--- Reading Data & Preparing LoRaWAN Payload ---");

    // Grid Power
    memcpy(appData + offset, &inverterData.gridPower, 4);
    Serial.printf("[Success] Grid Power: %.2f W | Bytes %d-%d: %02X %02X %02X %02X\n", 
                  inverterData.gridPower, offset, offset + 3, 
                  appData[offset], appData[offset+1], appData[offset+2], appData[offset+3]);
    offset += 4;

    // Grid Voltage
    memcpy(appData + offset, &inverterData.gridVoltage, 4);
    Serial.printf("[Success] Grid Voltage: %.2f V | Bytes %d-%d: %02X %02X %02X %02X\n", 
                  inverterData.gridVoltage, offset, offset + 3, 
                  appData[offset], appData[offset+1], appData[offset+2], appData[offset+3]);
    offset += 4;

    // PV Voltage
    memcpy(appData + offset, &inverterData.pvVoltage, 4);
    Serial.printf("[Success] PV Voltage: %.2f V | Bytes %d-%d: %02X %02X %02X %02X\n", 
                  inverterData.pvVoltage, offset, offset + 3, 
                  appData[offset], appData[offset+1], appData[offset+2], appData[offset+3]);
    offset += 4;

    // Inverter Temperature
    memcpy(appData + offset, &inverterData.inverterTemperature, 4);
    Serial.printf("[Success] Inverter Temp: %.2f C | Bytes %d-%d: %02X %02X %02X %02X\n", 
                  inverterData.inverterTemperature, offset, offset + 3, 
                  appData[offset], appData[offset+1], appData[offset+2], appData[offset+3]);
    offset += 4;

    // Daily Energy
    memcpy(appData + offset, &inverterData.dailyEnergy, 4);
    Serial.printf("[Success] Daily Energy: %u Wh | Bytes %d-%d: %02X %02X %02X %02X\n", 
                  inverterData.dailyEnergy, offset, offset + 3, 
                  appData[offset], appData[offset+1], appData[offset+2], appData[offset+3]);
    offset += 4;
    
    // Global State
    appData[offset] = inverterData.globalState;
    Serial.printf("[Success] Global State: %u | Byte %d: %02X\n", 
                  inverterData.globalState, offset, appData[offset]);
    offset++;

    // Alarm State
    appData[offset] = inverterData.alarmState;
    Serial.printf("[Success] Alarm State: %u | Byte %d: %02X\n", 
                  inverterData.alarmState, offset, appData[offset]);
    offset++;

    // Last Alarms
    memcpy(appData + offset, inverterData.lastAlarms, 4);
    Serial.printf("[Success] Last Alarms: | Bytes %d-%d: %02X %02X %02X %02X\n", 
                  offset, offset + 3, 
                  appData[offset], appData[offset+1], appData[offset+2], appData[offset+3]);
    offset += 4;

    // Part Number
    memcpy(appData + offset, inverterData.partNumber, 6);
    Serial.printf("[Success] Part Number: %s | Bytes %d-%d: %02X %02X %02X %02X %02X %02X\n", 
                  inverterData.partNumber, offset, offset + 5, 
                  appData[offset], appData[offset+1], appData[offset+2], appData[offset+3], appData[offset+4], appData[offset+5]);
    offset += 6;

    // Serial Number
    memcpy(appData + offset, inverterData.serialNumber, 6);
    Serial.printf("[Success] Serial Number: %s | Bytes %d-%d: %02X %02X %02X %02X %02X %02X\n", 
                  inverterData.serialNumber, offset, offset + 5, 
                  appData[offset], appData[offset+1], appData[offset+2], appData[offset+3], appData[offset+4], appData[offset+5]);
    offset += 6;
    
    // Alarm check
    if (inverterData.alarmState != 0) {
        isTxConfirmed = true;
        Serial.println("!!! Active Alarm Detected! Setting message to CONFIRMED. !!!");
    } else {
        isTxConfirmed = false;
    }

    Serial.printf("\nPrepared Payload (%d bytes) for FPort %d\n", appDataSize, appPort);
    Serial.print("Final Payload (Hex): ");
    for(int i=0; i<appDataSize; i++) {
        Serial.printf("%02X ", appData[i]);
    }
    Serial.println();
}

// =================================================================
//  Main Program Setup and Loop
// =================================================================
void setup() {
    Serial.begin(115200);
    Mcu.begin(HELTEC_BOARD, SLOW_CLK_TPYE);
    Serial2.begin(19200, SERIAL_8N1, 18, 17);
    inverter.begin();

    Serial.println("--- Heltec Comprehensive Inverter Monitor with LoRaWAN ---");
    Serial.println("Setup complete. Initializing LoRaWAN...");

    deviceState = DEVICE_STATE_INIT;
}

void loop() {
    switch (deviceState) {
        case DEVICE_STATE_INIT: {
            #if(LORAWAN_DEVEUI_AUTO)
            LoRaWAN.generateDeveuiByChipID();
            #endif
            LoRaWAN.init(loraWanClass, loraWanRegion);
            // 修改3: 删除固定数据速率设置
            // LoRaWAN.setDefaultDR(5); // 移除固定数据速率设置，启用ADR后将自动调整
            break;
        }
        case DEVICE_STATE_JOIN: {
            LoRaWAN.join();
            break;
        }
        case DEVICE_STATE_SEND: {
            bool read_ok = readAllInverterData();
            if (read_ok) {
                prepareTxFrame();
                LoRaWAN.send();
            } else {
                Serial.println("Skipping LoRaWAN transmission due to read failure.");
            }
            deviceState = DEVICE_STATE_CYCLE;
            break;
        }
        case DEVICE_STATE_CYCLE: {
            txDutyCycleTime = appTxDutyCycle + randr(-APP_TX_DUTYCYCLE_RND, APP_TX_DUTYCYCLE_RND);
            LoRaWAN.cycle(txDutyCycleTime);
            deviceState = DEVICE_STATE_SLEEP;
            break;
        }
        case DEVICE_STATE_SLEEP: {
            // MODIFIED: Restored the proper sleep function. This is critical for the LoRaWAN timing.
            LoRaWAN.sleep(loraWanClass); 
            break;
        }
        default: {
            deviceState = DEVICE_STATE_INIT;
            break;
        }
    }
}