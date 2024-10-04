#include "esp_camera.h"
#include "esp_log.h"
#include "dl_image.hpp"
#include "fb_gfx.h"
#include "image_util.hpp"
#include "esp_timer.h"
#include "fd_forward.hpp"
#include "fr_forward.hpp"
#include "human_face_detect_msr01.hpp"
#include "human_face_detect_mnp01.hpp"
#include "human_face_recognition.hpp"
#include <TFT_eSPI.h> // Assuming you are using a TFT screen with the TFT_eSPI library

#define PWDN_GPIO_NUM    -1
#define RESET_GPIO_NUM   -1
#define XCLK_GPIO_NUM    21
#define SIOD_GPIO_NUM    26
#define SIOC_GPIO_NUM    27

#define Y9_GPIO_NUM      35
#define Y8_GPIO_NUM      34
#define Y7_GPIO_NUM      39
#define Y6_GPIO_NUM      36
#define Y5_GPIO_NUM      19
#define Y4_GPIO_NUM      18
#define Y3_GPIO_NUM      5
#define Y2_GPIO_NUM      4
#define VSYNC_GPIO_NUM   25
#define HREF_GPIO_NUM    23
#define PCLK_GPIO_NUM    22

// TFT screen pins
#define TFT_CS   15
#define TFT_RST  2
#define TFT_DC   4
#define TFT_MOSI 13
#define TFT_SCLK 14

// Initialize the TFT screen
TFT_eSPI tft = TFT_eSPI(); // Create an instance of the TFT_eSPI class

static const char *TAG = "example:detect";

void setup() {
  Serial.begin(115200);

  // Camera configuration
  camera_config_t config;
  config.ledc_channel = LEDC_CHANNEL_0;
  config.ledc_timer = LEDC_TIMER_0;
  config.pin_d0 = Y2_GPIO_NUM;
  config.pin_d1 = Y3_GPIO_NUM;
  config.pin_d2 = Y4_GPIO_NUM;
  config.pin_d3 = Y5_GPIO_NUM;
  config.pin_d4 = Y6_GPIO_NUM;
  config.pin_d5 = Y7_GPIO_NUM;
  config.pin_d6 = Y8_GPIO_NUM;
  config.pin_d7 = Y9_GPIO_NUM;
  config.pin_xclk = XCLK_GPIO_NUM;
  config.pin_pclk = PCLK_GPIO_NUM;
  config.pin_vsync = VSYNC_GPIO_NUM;
  config.pin_href = HREF_GPIO_NUM;
  config.pin_sccb_sda = SIOD_GPIO_NUM;
  config.pin_sccb_scl = SIOC_GPIO_NUM;
  config.pin_pwdn = PWDN_GPIO_NUM;
  config.pin_reset = RESET_GPIO_NUM;
  config.xclk_freq_hz = 20000000;
  config.pixel_format = PIXFORMAT_RGB565; // Use RGB565 for direct display

  // Frame size
  config.frame_size = FRAMESIZE_QVGA;
  config.jpeg_quality = 12;
  config.fb_count = 1;

  // Initialize the camera
  esp_err_t err = esp_camera_init(&config);
  if (err != ESP_OK) {
    Serial.printf("Camera init failed with error 0x%x", err);
    return;
  }

  // Initialize the face detection model
  dl::detect::init();

  // Initialize the TFT screen
  tft.init();
  tft.setRotation(0); // Set the rotation to vertical
  tft.fillScreen(TFT_BLACK); // Clear the screen
}

void applyRedTint(uint16_t* buf, int width, int height) {
  for (int y = 0; y < height; y++) {
    for (int x = 0; x < width; x++) {
      uint16_t pixel = buf[y * width + x];
      uint8_t r = (pixel >> 11) & 0x1F;
      uint8_t g = (pixel >> 5) & 0x3F;
      uint8_t b = pixel & 0x1F;

      // Increase the red component
      r = 0x1F; // Max red value

      // Combine back to RGB565 format
      buf[y * width + x] = (r << 11) | (g << 5) | b;
    }
  }
}

void loop() {
  // Capture a frame
  camera_fb_t * fb = esp_camera_fb_get();
  if (!fb) {
    Serial.println("Camera capture failed");
    return;
  }

  // Convert the frame buffer to RGB format
  dl::image::rgb565_to_rgb888(fb->buf, fb->width, fb->height);

  // Run the detection model
  std::vector<dl::detect::box_t> boxes;
  dl::detect::detect(fb->buf, fb->width, fb->height, boxes);

  // Process the detection results
  bool person_detected = false;
  for (auto &box : boxes) {
    if (box.label == "person") {
      person_detected = true;
      Serial.printf("Person detected at [%d, %d, %d, %d]\n", box.x, box.y, box.w, box.h);
      // Draw a rectangle around the detected person
      fb_gfx_drawFastHLine(fb, box.x, box.y, box.w, 0xFFFF);
      fb_gfx_drawFastHLine(fb, box.x, box.y + box.h, box.w, 0xFFFF);
      fb_gfx_drawFastVLine(fb, box.x, box.y, box.h, 0xFFFF);
      fb_gfx_drawFastVLine(fb, box.x + box.w, box.y, box.h, 0xFFFF);
    }
  }

  // Apply red tint to the entire frame
  applyRedTint((uint16_t*)fb->buf, fb->width, fb->height);

  // Display the image on the screen
  tft.pushImage(0, 0, tft.width(), tft.height(), (uint16_t*)fb->buf);

  // Return the frame buffer back to the driver for reuse
  esp_camera_fb_return(fb);

  // Hold the outline on the screen for 1 second if a person is detected
  if (person_detected) {
    delay(1000);
  } else {
    delay(30); // Adjust the delay as needed for smoother video
  }
}