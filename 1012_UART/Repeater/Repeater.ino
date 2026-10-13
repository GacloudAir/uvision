// RP2040 USB <-> UART1 透传程序
// 适用于 Arduino-Pico 核心

void setup() {
  // 初始化USB串口 (用于与电脑通信)
  Serial.begin(115200);
  
  // 配置UART1 (Serial2) 的引脚为 GPIO4 (TX) 和 GPIO5 (RX)
  Serial2.setTX(8);
  Serial2.setRX(9);
  
  // 初始化UART1，波特率需与C8051F350一致 (例程中为9600)
  Serial2.begin(9600);
}

void loop() {
  // 电脑 -> C8051F350（同时回显）
  while (Serial.available()) {
    char c = Serial.read();   // 只读一次
    Serial2.write(c);         // 转发给 8051
    Serial.write(c);          // 回显到电脑（可选）
  }

  // C8051F350 -> 电脑
  while (Serial2.available()) {
    Serial.write(Serial2.read());
  }
}