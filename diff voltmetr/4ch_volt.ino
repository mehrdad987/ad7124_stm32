#include <SPI.h>

// --------------------------
// Pin Assignments (Blue Pill)
// --------------------------
#define CS_PIN   PB0
#define SCK_PIN  PA5
#define MISO_PIN PA6
#define MOSI_PIN PA7

// AD7124 Commands / Registers
#define AD7124_COMM_READ     0x40
#define AD7124_COMM_WRITE    0x00
#define AD7124_STATUS_REG    0x00
#define AD7124_CTRL_REG      0x01
#define AD7124_DATA_REG      0x02
#define AD7124_ID_REG        0x05
#define AD7124_CH0_REG       0x09
#define AD7124_CFG0_REG      0x19
#define AD7124_FILT0_REG     0x21

const int32_t VREF_MV = 2500;      // internal 2.5 V reference
SPIClass spi(MOSI_PIN, MISO_PIN, SCK_PIN);

// CONTINUOUS (old ver): 0x058C
const uint16_t CTRL_SINGLE = 0x050C;

// ==================================================
// SPI helpers
// ==================================================
void spiWriteReg(uint8_t reg, uint32_t value, uint8_t bytes) {
  digitalWrite(CS_PIN, LOW);
  spi.transfer(AD7124_COMM_WRITE | reg);
  for (int i = bytes - 1; i >= 0; --i) {
    spi.transfer((value >> (8 * i)) & 0xFF);
  }
  digitalWrite(CS_PIN, HIGH);
}

uint32_t spiReadReg(uint8_t reg, uint8_t bytes) {
  digitalWrite(CS_PIN, LOW);
  spi.transfer(AD7124_COMM_READ | reg);
  uint32_t val = 0;
  for (int i = 0; i < bytes; ++i) {
    val = (val << 8) | spi.transfer(0x00);
  }
  digitalWrite(CS_PIN, HIGH);
  return val;
}

// Convert 24-bit raw reading → millivolts (unipolar)

int32_t raw_to_mV_unipolar(uint32_t raw24, int32_t vref_mV) {
  uint64_t tmp = (uint64_t)raw24 * vref_mV + (0xFFFFFF / 2);
  return (int32_t)(tmp / 0xFFFFFF);
}

// Reset and configure AD7124

void ad7124Reset() {
  digitalWrite(CS_PIN, LOW);
  for (int i = 0; i < 8; i++) spi.transfer(0xFF);
  digitalWrite(CS_PIN, HIGH);
  delay(5);
}

void ad7124Init() {
  Serial.println("Resetting AD7124…");
  ad7124Reset();
  delay(10);

  uint32_t id = spiReadReg(AD7124_ID_REG, 1);
  Serial.printf("ID = 0x%02X\n", (unsigned)id);

  Serial.println("Configuring AD7124…");

  // Control register: leave in single-conversion mode by default
  spiWriteReg(AD7124_CTRL_REG, CTRL_SINGLE, 2);

  // Config0: unipolar, gain=1, internal reference
  spiWriteReg(AD7124_CFG0_REG, 0x0860, 2);

  // Filter0: SINC3, medium rate
  spiWriteReg(AD7124_FILT0_REG, 0x0600, 2);

  // Start with all channels disabled
  for (uint8_t c = 0; c < 8; c++) {
    spiWriteReg(AD7124_CH0_REG + c, 0x0000, 2);
  }

  delay(50);
}
// AIN0/1 => CH0, AIN2/3 => CH1, AIN4/5 => CH2, AIN6/7 => CH3
void enableChannel(uint8_t ch, uint8_t ainp, uint8_t ainm) {
  uint16_t val =
      0x8000 |          // enable
      (0 << 12) |       // use Config0 (CFG0)
      (ainp << 5) |     // positive input
      (ainm);           // negative input

  spiWriteReg(AD7124_CH0_REG + ch, val, 2);
}

void disableAllChannels() {
  for (uint8_t c = 0; c < 8; c++) {
    spiWriteReg(AD7124_CH0_REG + c, 0x0000, 2);
  }
}
// - Uses single-conversion mode: write CTRL to start

bool read_one_channel(uint8_t ch, uint32_t &raw_out, int32_t &mv_out, uint32_t timeout_ms = 1000) {
  // Map channel to AIN pair (per your wiring)
  uint8_t ainp = ch * 2;      // 0->AIN0, 1->AIN2, 2->AIN4, 3->AIN6
  uint8_t ainm = ch * 2 + 1;  // 0->AIN1, 1->AIN3, 2->AIN5, 3->AIN7

  // Prepare channel
  disableAllChannels();
  enableChannel(ch, ainp, ainm);

  // Small settle before starting conversion (filter warm-up)
  delay(10);

  // Start a single conversion explicitly
  spiWriteReg(AD7124_CTRL_REG, CTRL_SINGLE, 2);

  // Poll STATUS for Data Ready (bit7). We'll wait until DRDY clears.
  uint32_t start = millis();
  while (true) {
    uint8_t status = (uint8_t)spiReadReg(AD7124_STATUS_REG, 1);
    // Datasheet: STATUS bit7 (RDY) = 0 when data ready. Wait until it's 0.
    if ((status & 0x80) == 0) break;
    if (millis() - start >= timeout_ms) {
      // timeout
      return false;
    }
    delay(2);
  }

  // Read the 24-bit data
  uint32_t raw = spiReadReg(AD7124_DATA_REG, 3) & 0xFFFFFF;
  int32_t mv = raw_to_mV_unipolar(raw, VREF_MV);

  raw_out = raw;
  mv_out = mv;
  return true;
}

void setup() {
  Serial.begin(115200);
  delay(300);

  pinMode(CS_PIN, OUTPUT);
  digitalWrite(CS_PIN, HIGH);

  spi.begin();
  spi.beginTransaction(SPISettings(100000, MSBFIRST, SPI_MODE3));

  ad7124Init();
  Serial.println("Ready. Reading channels: CH0(AIN0-1), CH1(AIN2-3), CH2(AIN4-5), CH3(AIN6-7)");
}

void loop() {
  for (uint8_t ch = 0; ch < 4; ch++) {
    uint32_t raw;
    int32_t mv;
    
    bool ok = read_one_channel(ch, raw, mv, 1000);
    if (ok) {
      mv=((mv-1250)*2);
      Serial.printf("CH%u Raw:%06X  %ld mV\n", (unsigned)ch, (unsigned)raw, (long)mv);
    } else {
      Serial.printf("CH%u Timeout/error reading\n", (unsigned)ch);
    }
    delay(20); // short gap between channel reads
  }
  Serial.println("-----------------------------");
  delay(400);
}
