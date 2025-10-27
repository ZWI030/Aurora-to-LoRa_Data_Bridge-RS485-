/**
 * @file Aurora_Simple_Reader.ino
 * @brief Aurora逆变器参数读取与串口打印测试程序
 * @version 1.1
 * @date 2025-09-29
 *
 * 功能:
 * 1. 通过RS485 (Serial2) 连接到一台Aurora逆变器。
 * 2. 周期性地读取逆变器的状态、实时数据、累计数据和设备信息。
 * 3. 将读取结果通过USB串口 (Serial) 打印到电脑，用于现场通信测试。
 *
 * 现场测试注意事项:
 * 1. 在代码中核对并修改 INVERTER_ADDRESS 以匹配真实逆变器的地址。
 * 2. 确认RS485接线正确 (A -> +T/R, B -> -T/R, GND -> RTN)。
 * 3. 使用稳定电源为本设备供电。
 */

#include "Aurora.h"

// =================================================================
//  Configuration for Aurora Inverter (RS485)
// =================================================================

// !!! Confirm the address
#define INVERTER_ADDRESS 2

// 用于RS485通信的Serial2引脚 (Heltec Wireless Stick Lite V3)
#define RS485_RX_PIN 18
#define RS485_TX_PIN 17

// 当使用自动流控RS485模块时，此引脚不实际使用，仅作占位符
#define UNUSED_FLOW_CONTROL_PIN 4       

// 初始化Aurora逆变器通信对象
// 使用Serial2进行RS485通信
Aurora inverter(INVERTER_ADDRESS, &Serial2, UNUSED_FLOW_CONTROL_PIN);

// =================================================================
//  程序主函数
// =================================================================

void setup() {
    // 启动USB串口用于调试信息输出
    Serial.begin(115200);
    
    // 等待串口连接
    delay(2000); 

    Serial.println("\n--- Aurora Inverter Simple Reader Test (Final Version) ---");
    Serial.println("Initializing...");

    // 启动用于RS485通信的硬件串口2
    // 波特率19200, 8数据位, 无校验, 1停止位
    Serial2.begin(19200, SERIAL_8N1, RS485_RX_PIN, RS485_TX_PIN);

    // 初始化Aurora库 (此函数内部会设置流控制引脚模式)
    inverter.begin();

    Serial.printf("Setup complete. Attempting to communicate with inverter at address %d.\n", INVERTER_ADDRESS);
    Serial.println("Starting periodic reads in 3 seconds...");
    delay(3000);
}

void loop() {
    // 调用核心函数，读取并打印所有数据
    readAndPrintInverterData();

    // 打印分隔符，方便阅读
    Serial.println("\n-----------------------------------\n");

    // 等待5秒后进行下一次读取
    delay(5000);
}

// =================================================================
//  数据采集与打印核心功能函数
// =================================================================

void readAndPrintInverterData() {
    Serial.println("--- Starting New Read Cycle ---");

    // 1. 读取状态信息 (Command 50)
    Aurora::DataState state = inverter.readState();
    if (state.state.readState) {
        Serial.printf("  [OK] Global State: %d (%s)\n", state.state.globalState, state.state.getGlobalState().c_str());
        Serial.printf("  [OK] Inverter State: %d (%s)\n", state.inverterState, state.getInverterState().c_str());
        Serial.printf("  [OK] Alarm State: %d (%s)\n", state.alarmState, state.getAlarmState().c_str());
    } else {
        Serial.println("  [FAIL] Failed to read State.");
    }

    // 2. 读取DSP实时测量值 (Command 59)
    Aurora::DataDSP dsp;
    dsp = inverter.readDSP(3, 0); // Grid Power
    if (dsp.state.readState) { Serial.printf("  [OK] Grid Power: %.2f W\n", dsp.value); } else { Serial.println("  [FAIL] Failed to read Grid Power."); }
    
    dsp = inverter.readDSP(1, 0); // Grid Voltage
    if (dsp.state.readState) { Serial.printf("  [OK] Grid Voltage: %.2f V\n", dsp.value); } else { Serial.println("  [FAIL] Failed to read Grid Voltage."); }
    
    dsp = inverter.readDSP(23, 0); // PV Voltage
    if (dsp.state.readState) { Serial.printf("  [OK] PV Voltage (Input 1): %.2f V\n", dsp.value); } else { Serial.println("  [FAIL] Failed to read PV Voltage."); }
    
    dsp = inverter.readDSP(21, 0); // Inverter Temperature
    if (dsp.state.readState) { Serial.printf("  [OK] Inverter Temperature: %.2f C\n", dsp.value); } else { Serial.println("  [FAIL] Failed to read Inverter Temp."); }

    // 3. 读取累计发电量 (Command 78)
    Aurora::DataCumulatedEnergy energy = inverter.readCumulatedEnergy(0); // Daily Energy
    if (energy.state.readState) { Serial.printf("  [OK] Daily Energy: %lu Wh\n", energy.energy); } else { Serial.println("  [FAIL] Failed to read Daily Energy."); }

    // 4. 读取最后四次告警 (Command 86)
    Aurora::DataLastFourAlarms alarms = inverter.readLastFourAlarms();
    if (alarms.state.readState) {
        Serial.printf("  [OK] Last 4 Alarms (newest to oldest): %d, %d, %d, %d\n", alarms.alarm4, alarms.alarm3, alarms.alarm2, alarms.alarm1);
    } else {
        Serial.println("  [FAIL] Failed to read Last Four Alarms.");
    }

    // 5. 读取部件号 (PN - Command 52)
    Aurora::DataSystemPN pn = inverter.readSystemPN();
    if (pn.readState) { Serial.printf("  [OK] Part Number: %s\n", pn.PN.c_str()); } else { Serial.println("  [FAIL] Failed to read Part Number."); }

    // 6. 读取序列号 (SN - Command 63)
    Aurora::DataSystemSerialNumber sn = inverter.readSystemSerialNumber();
    if (sn.readState) { Serial.printf("  [OK] Serial Number: %s\n", sn.SerialNumber.c_str()); } else { Serial.println("  [FAIL] Failed to read Serial Number."); }
}