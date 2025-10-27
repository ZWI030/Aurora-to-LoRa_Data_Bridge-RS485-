// --- Heltec UART2 Loopback Test Code ---

#define RS485_RX_PIN 18 // Serial2 RX pin
#define RS485_TX_PIN 17 // Serial2 TX pin

void setup() {
  // 启动USB串口用于调试输出
  Serial.begin(115200);
  delay(2000);
  Serial.println("--- UART2 Loopback Test ---");

  // 启动用于测试的硬件串口2
  Serial2.begin(19200, SERIAL_8N1, RS485_RX_PIN, RS485_TX_PIN);
  Serial.println("Serial2 started on GPIO 17 (TX) and GPIO 18 (RX).");
  Serial.println("Now sending data from TX2 and trying to receive on RX2...");
}

void loop() {
  // 1. 通过Serial2的TX引脚发送一个测试消息
  String messageToSend = "Hello, can you hear me?";
  Serial2.println(messageToSend);
  Serial.printf("\nMessage Sent via TX2: '%s'\n", messageToSend.c_str());

  // 2. 稍微等待，给数据回环留出时间
  delay(100); 

  // 3. 检查Serial2的RX引脚是否收到了数据
  if (Serial2.available()) {
    String receivedMessage = Serial2.readStringUntil('\n');
    Serial.printf("  [SUCCESS] Message Received via RX2: '%s'\n", receivedMessage.c_str());
  } else {
    Serial.println("  [FAIL] No data received on RX2. Check the connection between pin 17 and 18!");
  }

  // 4. 等待5秒进行下一次测试
  delay(5000);
}