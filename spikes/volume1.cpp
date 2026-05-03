//* Test INMP441 I2S microphone on ESP32 and ESP32-C3
//* Display RMS and dB (negative - digital relative dB -> dBFS) values in Serial Monitor

#include <Arduino.h>
#include <driver/i2s.h>
#include <math.h>

// ---------------- I2S CONFIG ----------------
#if defined(CONFIG_IDF_TARGET_ESP32C3)
#define I2S_WS 7  // L/R clock
#define I2S_SD 3  // data in
#define I2S_SCK 6 // bit clock
#elif defined(CONFIG_IDF_TARGET_ESP32)
#define I2S_WS 25
#define I2S_SD 32
#define I2S_SCK 33
#endif

#define I2S_PORT I2S_NUM_0
#define SAMPLE_RATE 16000
#define BUFFER_LEN 512

#if defined(CONFIG_IDF_TARGET_ESP32C3)
const byte pinBtn = 9; // boot button
#elif defined(CONFIG_IDF_TARGET_ESP32)
const byte pinBtn = 18;
#endif
bool isPaused = false;

// int16_t sBuffer[BUFFER_LEN];
int32_t sBuffer[BUFFER_LEN];

// ---------------- SETUP ----------------
void setup()  
{
  Serial.begin(115200);
  delay(100);
  pinMode(pinBtn, INPUT_PULLUP);

  i2s_config_t i2s_config = {
      .mode = (i2s_mode_t)(I2S_MODE_MASTER | I2S_MODE_RX),
      .sample_rate = SAMPLE_RATE,
      // .bits_per_sample = I2S_BITS_PER_SAMPLE_16BIT,
      .bits_per_sample = I2S_BITS_PER_SAMPLE_32BIT,
      .channel_format = I2S_CHANNEL_FMT_ONLY_LEFT, // INMP441 usually uses LEFT
      .communication_format = i2s_comm_format_t(I2S_COMM_FORMAT_STAND_I2S),
      .intr_alloc_flags = 0,
      .dma_buf_count = 4,
      .dma_buf_len = 256,
      .use_apll = false};

  i2s_pin_config_t pin_config = {
      .bck_io_num = I2S_SCK,
      .ws_io_num = I2S_WS,
      .data_out_num = I2S_PIN_NO_CHANGE,
      .data_in_num = I2S_SD};

  Serial.println(i2s_driver_install(I2S_PORT, &i2s_config, 0, NULL));
  Serial.println(i2s_set_pin(I2S_PORT, &pin_config));
  Serial.println(i2s_zero_dma_buffer(I2S_PORT));
  Serial.println("I2S initialized.");
}

// ---------------- RMS CALC ----------------
float calculateRMS(int32_t *buffer, int samples)
{
  double sum = 0.0;
  // double mean = 0.0;
  // // ---- Calculate mean (DC offset) ----
  // for (int i = 0; i < samples; i++)
  //     mean += buffer[i];
  // mean /= samples;

  // ---- Calculate RMS ----
  for (int i = 0; i < samples; i++)
  {
    // float sample = buffer[i] - mean;
    float sample = (buffer[i] >> 8); // align 24-bit
    // INMP441: 24-bit data inside 32-bit container
    sample = sample / 8388608.0; // 2^23
    sum += sample * sample;
  }

  float rms = sqrt(sum / samples);
  return rms;
}

float calculateRMS_16bit(int16_t *buffer, int samples)
{
  double sum = 0.0;
  double mean = 0.0;

  // ---- DC offset removal ----
  for (int i = 0; i < samples; i++)
  {
    mean += buffer[i];
  }
  mean /= samples;

  // ---- RMS calculation ----
  for (int i = 0; i < samples; i++)
  {
    float sample = buffer[i] - mean;

    // normalize to -1.0 ... +1.0
    sample = sample / 32768.0;

    sum += sample * sample;
  }

  return sqrt(sum / samples);
}

// ---------------- DB CONVERSION ----------------
float rmsToDb(float rms)
{
  if (rms < 1e-7)
    return -100.0; // avoid log(0)
  return 20.0 * log10(rms);
}

// ---------------- LOOP ----------------
void loop()
{
  delay(200);
  if (!digitalRead(pinBtn))
  {
    isPaused = !isPaused;
    Serial.println(isPaused ? "Paused" : "Resumed");
    delay(500); // debounce
  }
  if (isPaused)
    return;

  size_t bytesRead = 0;
  auto res = i2s_read(I2S_PORT, (void *)sBuffer, sizeof(sBuffer), &bytesRead, portMAX_DELAY);
  if (res != ESP_OK)
  {
    // Serial.print("I2S read error: ");
    // Serial.println(res);
    // write error code in HEX for easier debugging
    printf("I2S read error: %x\n", res);
    delay(1000);
    return;
  }
  //* count all valus in buffer != 0 (debugging)
  // int nonZeroCount = 0;
  // for (int i = 0; i < bytesRead / sizeof(int32_t); i++)
  // {
  //     if (sBuffer[i] != 0)
  //         nonZeroCount++;
  // }
  // if (nonZeroCount != 0)
  //     Serial.println(nonZeroCount);

  // int samples = bytesRead / sizeof(int16_t);
  int samples = bytesRead / sizeof(int32_t);
  float rms = calculateRMS(sBuffer, samples);
  // if (rms < 0.002)
  //     rms = 0;
  // if (rms > 0.1)
  {
    float db = rmsToDb(rms);
    printf("RMS: %.5f   dB: %.0f\n", rms, db);
    // printf("RMS: %.5f   dB: %.0f   Bytes Read: %d\n", rms, db, bytesRead);
  }
}
