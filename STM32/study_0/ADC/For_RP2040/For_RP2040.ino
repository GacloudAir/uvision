#define DAC_PIN 0

void setup() {
    analogWriteResolution(10);        // 分辨率 10 位: 取值 0~1023
    analogWriteFreq(125000);          // 载波提到 125kHz, 配 RC 后更平滑(可选但推荐)
}

void loop() {
    // 三角波扫描 0V -> 3.3V -> 0V, 每步约4ms
    for (int lvl = 0; lvl <= 1023; lvl += 4) {
        analogWrite(DAC_PIN, lvl);
        delay(4);
    }
    for (int lvl = 1023; lvl >= 0; lvl -= 4) {
        analogWrite(DAC_PIN, lvl);
        delay(4);
    }
}

