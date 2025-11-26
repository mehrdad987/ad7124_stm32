#include <SPI.h>

#define CS_PIN   PB0
#define SCK_PIN  PA5
#define MISO_PIN PA6
#define MOSI_PIN PA7

#define REG_STATUS      0x00
#define REG_ADC_CONTROL 0x01
#define REG_DATA        0x02
#define REG_IO_CONTROL1 0x03  // 24-bit register
#define REG_ID          0x05
#define REG_CHANNEL_0   0x09
#define REG_CONFIG_0    0x19
#define REG_FILTER_0    0x21

const int32_t VREF_MV = 2500;
SPIClass SPI_2(MOSI_PIN, MISO_PIN, SCK_PIN);

// Communication commands
#define WRITE_CMD(reg)  (0x00 | (reg))
#define READ_CMD(reg)   (0x40 | (reg))

bool writeAndVerify(uint8_t reg, uint32_t value, uint8_t bytes, const char* name) {
  writeReg(reg, value, bytes);
  delay(2);
  uint32_t readback = readReg(reg, bytes);
  bool ok = (readback == value);
  Serial.printf("%s = 0x%0*lX ", name, bytes * 2, value);
  if (ok) {
    Serial.println("OK");
  } else {
    Serial.printf("FAILED! Read 0x%0*lX\n", bytes * 2, readback);
    while (1) delay(100);
  }
  return ok;
}

void writeReg(uint8_t reg, uint32_t value, uint8_t bytes) {
  digitalWrite(CS_PIN, LOW);
  SPI_2.transfer(WRITE_CMD(reg));
  for (int i = bytes - 1; i >= 0; --i) {
    SPI_2.transfer((value >> (i * 8)) & 0xFF);
  }
  digitalWrite(CS_PIN, HIGH);
}

uint32_t readReg(uint8_t reg, uint8_t bytes) {
  digitalWrite(CS_PIN, LOW);
  SPI_2.transfer(READ_CMD(reg));
  uint32_t val = 0;
  for (uint8_t i = 0; i < bytes; i++) {
    val = (val << 8) | SPI_2.transfer(0x00);
  }
  digitalWrite(CS_PIN, HIGH);
  return val;
}

int32_t rawToMv(uint32_t raw24) {
  return (int32_t)((uint64_t)raw24 * VREF_MV / 0xFFFFFF);
}

void setup() {
  Serial.begin(115200);
  while (!Serial);

  pinMode(CS_PIN, OUTPUT);
  digitalWrite(CS_PIN, HIGH);

  SPI_2.begin();
  SPI_2.beginTransaction(SPISettings(1000000, MSBFIRST, SPI_MODE3));

  // Reset
  digitalWrite(CS_PIN, LOW);
  for (int i = 0; i < 8; i++) SPI_2.transfer(0xFF);
  digitalWrite(CS_PIN, HIGH);
  delay(10);

  Serial.println(F("\n=== AD7124-4 ===\n"));

  uint8_t id = readReg(REG_ID, 1);
  Serial.printf("ID = 0x00\n", id);

  writeAndVerify(REG_CONFIG_0, 0x0060, 2, "CONFIG_0 (unipolar)");
  writeAndVerify(REG_FILTER_0, 0x060180, 3, "FILTER_0 (SINC3 ~50 SPS)");
  writeAndVerify(REG_IO_CONTROL1, 0x0024FE, 3, "IO_CONTROL1 (250uA AIN6/AIN7)");
  writeAndVerify(REG_ADC_CONTROL, 0x000C, 2, "ADC_CONTROL (power on)");
  for (uint8_t ch = 0; ch < 8; ch++) writeReg(REG_CHANNEL_0 + ch, 0x0000, 2);
  writeAndVerify(REG_CHANNEL_0, 0x8001, 2, "CHANNEL_0 (AIN0-AIN1)");
  
}

void loop() {
  writeReg(REG_ADC_CONTROL, 0x050C, 2);  // Single conversion
  while (readReg(REG_STATUS, 1) & 0x80) delay(1);
  int32_t mv = rawToMv(readReg(REG_DATA, 3) & 0xFFFFFF);
  Serial.printf("AIN0,1: %ld mV AIN6,7: 500uA \n", mv);
  delay(500);
}
