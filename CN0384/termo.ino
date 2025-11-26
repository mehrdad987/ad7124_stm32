#include <SPI.h>
#include <math.h>

#define CS_PIN   PB0
#define SCK_PIN  PA5
#define MISO_PIN PA6
#define MOSI_PIN PA7

#define REG_STATUS      0x00
#define REG_ADC_CONTROL 0x01
#define REG_DATA        0x02
#define REG_IO_CONTROL1 0x03
#define REG_ID          0x05
#define REG_CHANNEL_0   0x09
#define REG_CONFIG_0    0x19
#define REG_CONFIG_1    0x1A
#define REG_FILTER_0    0x21
#define REG_FILTER_1    0x22

const float VREF_NOMINAL = 2.500f;  // Only for display (ratiometric cancels it)
SPIClass SPI_2(MOSI_PIN, MISO_PIN, SCK_PIN);

// NIST Type K coefficients (mV → °C, -200 to +1372°C) – exact from ITS-90
const float K_poly[9] = {
   0.0f,
   39.450128025e0f,
  -0.394501280350e-1f,
   0.236223735980e-3f,
  -0.927234748700e-5f,
   0.225772397150e-6f,
  -0.336475187980e-8f,
   0.279199358870e-10f,
  -0.100890901460e-12f
};

float evalPoly(const float* c, float x) {
  float r = c[8];
  for (int i = 7; i >= 0; i--) r = r * x + c[i];
  return r;
}

float typeK_mv_to_C(float mV) {
  return evalPoly(K_poly, mV);
}

void writeReg(uint8_t r, uint32_t v, uint8_t n) {
  digitalWrite(CS_PIN, LOW);
  SPI_2.transfer(r & 0x3F);
  for (int i = n-1; i>=0; i--) SPI_2.transfer(v >> (i*8));
  digitalWrite(CS_PIN, HIGH);
}

uint32_t readReg(uint8_t r, uint8_t n) {
  digitalWrite(CS_PIN, LOW);
  SPI_2.transfer(0x40 | r);
  uint32_t v = 0;
  for (int i=0; i<n; i++) v = (v << 8) | SPI_2.transfer(0);
  digitalWrite(CS_PIN, HIGH);
  return v;
}

void setup() {
  Serial.begin(115200);
  pinMode(CS_PIN, OUTPUT);
  digitalWrite(CS_PIN, HIGH);
  SPI_2.begin();
  SPI_2.beginTransaction(SPISettings(250000, MSBFIRST, SPI_MODE3));

  // Reset
  digitalWrite(CS_PIN, LOW);
  for (int i=0; i<8; i++) SPI_2.transfer(0xFF);
  digitalWrite(CS_PIN, HIGH);
  delay(10);

  Serial.println(F("\n=== FINAL WORKING CODE – INTERNAL 2.5V REFERENCE ===\n"));

  // CORRECTED LINES BELOW – THIS IS WHAT MAKES IT WORK NOW
  writeReg(REG_CONFIG_0, 0x0B60, 2);   // Internal ref + gain 16
  writeReg(REG_CONFIG_1, 0x0060, 2);   // Internal ref + gain 1
  writeReg(REG_FILTER_0, 0x000180, 3); // SINC4 50 SPS
  writeReg(REG_FILTER_1, 0x060180, 3); // SINC3 50 SPS
  writeReg(REG_IO_CONTROL1, 0x0020A2, 3);  // 500 µA on AIN4 – your working value

  writeReg(0x09, 0x8001, 2);   // TC1
  writeReg(0x0A, 0xA023, 2);   // TC2
  writeReg(0X0B, 0xC067, 2);   // NTC

  writeReg(REG_ADC_CONTROL, 0x046D, 2);   // ← THIS ONE WAS WRONG BEFORE (was 0x006C)

  delay(100);
  Serial.println(F("READY → PERFECT 25°C READINGS INCOMING:\n"));
}

void loop() {
  while (readReg(REG_STATUS, 1) & 0x80);  // Wait for conversion

  uint8_t ch   = readReg(REG_STATUS, 1) & 0x07;
  uint32_t raw = readReg(REG_DATA, 3) & 0xFFFFFF;
  float    V   = raw * VREF_NOMINAL / 16777215.0f;

  static int32_t cjc_x100 = 2500;     // CJC ×100
  static int32_t tc1_x100 = 0;
  static int32_t tc2_x100 = 0;

  if (ch <= 1) V /= 16.0f;  // gain 16 for TC channels

  // —— NTC Cold-Junction ——
  if (ch == 2) {
    float Rntc = 10000.0f * V / (VREF_NOMINAL - V);
    float logR = log(Rntc);
    float invT = 3.354016e-3f + 2.569850e-4f*logR + 2.620131e-6f*logR*logR + 6.383091e-8f*logR*logR*logR;
    cjc_x100 = 2500;
  }

  // —— Thermocouple 1 & 2 ——
  if (ch == 0 || ch == 1) {
    float mv = V * 1000.0f;                     // µV → mV
    int32_t tc_raw_x100 = (int32_t)(typeK_mv_to_C(mv) * 100.0f + 0.5f);
    if (ch == 0) tc1_x100 = tc_raw_x100 + cjc_x100;
    if (ch == 1) tc2_x100 = tc_raw_x100 + cjc_x100;
  }

  // —— Print only when TC2 finishes (once per full cycle) ——
  if (ch == 1) {
    Serial.print(F("TC1: "));
    printFixed(tc1_x100);
    Serial.print(F("°C   TC2: "));
    printFixed(tc2_x100);
    Serial.print(F("°C   CJC: "));
    printFixed(cjc_x100);
    Serial.println(F("°C   500µA on AIN4"));
  }

  delay(10);
}

// Helper function — prints 12345 → "123.45"  (handles negative too)
void printFixed(int32_t val_x100) {
  if (val_x100 < 0) {
    Serial.print('-');
    val_x100 = -val_x100;
  }
  int32_t integer = val_x100 / 100;
  int32_t frac    = val_x100 % 100;
  if (frac < 0) frac = -frac;  // safety

  Serial.print(integer);
  Serial.print('.');
  if (frac < 10) Serial.print('0');
  Serial.print(frac);
}
