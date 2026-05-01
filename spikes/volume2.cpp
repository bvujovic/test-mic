

#include <Arduino.h>
#include <driver/i2s.h>

//* pins for ESP32-C3 SuperMini
// #define I2S_WS 0
// #define I2S_SD 2
// #define I2S_SCK 1
//* pins for ESP32
#define I2S_WS 25
#define I2S_SD 32
#define I2S_SCK 33

#define I2S_PORT I2S_NUM_0 // Use I2S Processor 0

#define bufferLen 64
int32_t sBuffer[bufferLen];

void i2s_install()
{
  // Set up I2S Processor configuration
  const i2s_config_t i2s_config = {
      .mode = i2s_mode_t(I2S_MODE_MASTER | I2S_MODE_RX),
      .sample_rate = 16000,
      .bits_per_sample = I2S_BITS_PER_SAMPLE_24BIT,
      .channel_format = I2S_CHANNEL_FMT_ONLY_LEFT,
      .communication_format = i2s_comm_format_t(I2S_COMM_FORMAT_STAND_I2S),
      .intr_alloc_flags = 0,
      .dma_buf_count = 8,
      .dma_buf_len = bufferLen,
      .use_apll = false};

  auto res = i2s_driver_install(I2S_PORT, &i2s_config, 0, NULL);
  Serial.println("i2s_driver_install result: " + String(res));
}

void i2s_setpin()
{
  // Set I2S pin configuration
  const i2s_pin_config_t pin_config = {
      .bck_io_num = I2S_SCK,
      .ws_io_num = I2S_WS,
      .data_out_num = -1,
      .data_in_num = I2S_SD};

  auto res = i2s_set_pin(I2S_PORT, &pin_config);
  Serial.println("i2s_set_pin result: " + String(res));
}

// float calculateRMS(int16_t *buffer, int samples)
float calculateRMS(int32_t *buffer, int samples)
{
  double sum = 0;

  for (int i = 0; i < samples; i++)
  {
    float sample = buffer[i];

    // Optional: shift if needed (common for INMP441)
    sample = sample / 8388608.0; // normalize 24-bit
    // sample = sample / 32768.0; // normalize 16-bit

    sum += sample * sample;
  }

  float mean = sum / samples;
  return sqrt(mean);
}

float rmsToDb(float rms)
{
  if (rms <= 0.000001)
    return -100.0; // avoid log(0)
  return 20.0 * log10(rms);
}

ulong ms;

void setup()
{
  // Set up Serial Monitor
  Serial.begin(115200);
  Serial.println();

  delay(1000);

  // Set up I2S
  i2s_install();
  i2s_setpin();
  auto res = i2s_start(I2S_PORT);
  Serial.println("i2s_start result: " + String(res));

  delay(500);
  ms = 0;
}

ulong msLastDotPrint = 0;
ulong msLastPrint = 0;

void loop()
{
  // if (millis() - ms < 10)
  //     return;
  // ms = millis();

  // False print statements to "lock range" on serial plotter display
  // Change rangelimit value to adjust "sensitivity"
  // int rangelimit = 3000;
  // Serial.print(rangelimit * -1);
  // Serial.print(" ");
  // Serial.print(rangelimit);
  // Serial.print(" ");

  // Get I2S data and place in data buffer
  size_t bytesIn = 0;
  esp_err_t result = i2s_read(I2S_PORT, &sBuffer, bufferLen, &bytesIn, portMAX_DELAY);

  if (result == ESP_OK)
  {
    // if (bytesIn >= 4)
    // {
    //   printf("%d %d %d %d\n", sBuffer[0], sBuffer[1], sBuffer[2], sBuffer[3]);
    // }
    // Read I2S data buffer
    int16_t samples_read = bytesIn / 4; // 4 bytes per sample for 32-bit
    // Serial.print("Bytes read: " + String(bytesIn));
    // Serial.println(" Samples read: " + String(samples_read));
    if (samples_read > 0)
    {
      auto sum = 0;
      for (int16_t i = 0; i < samples_read; ++i)
        sum += (sBuffer[i]);
      // auto mean = sum / samples_read;
      float rms = calculateRMS(sBuffer, samples_read);
      if (millis() - msLastPrint > 2000)
      {
        // printf("RMS: %0.4f\n", rms);
        printf("%d %d %d %d\n", sBuffer[0], sBuffer[1], sBuffer[2], sBuffer[3]);
        msLastPrint = millis();
      }

      if (rms > 0.1)
      // if (mean > 500 || mean < -500)
      {
        // Serial.println(mean);
        float db = rmsToDb(rms);
        // Serial.print("RMS: ");
        // Serial.print(rms);
        // Serial.print("\t\t dB: ");
        // Serial.println(db);
        if (db > 50)
          printf("RMS: %0.4f\t\t dB: %0.1f\n", rms, db);
      }
    }
  }
  // //* Testing if Serial monitor is alive - prints a dot every 5 seconds
  // if (millis() - msLastDotPrint > 5000)
  // {
  //   Serial.print('.');
  //   msLastDotPrint = millis();
  // }
}
