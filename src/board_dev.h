#pragma once
#include <Wire.h>
TaskHandle_t screen_update_handle;
TaskHandle_t client_cmd_handle;

// SSD1306
#include <Adafruit_SSD1306.h>
static constexpr int SCREEN_WIDTH   = 128;
static constexpr int SCREEN_HEIGHT  = 32;
static constexpr int OLED_RESET     = -1;
static constexpr int SCREEN_ADDRESS = 0x3C;
Adafruit_SSD1306 display(SCREEN_WIDTH, SCREEN_HEIGHT, &Wire, OLED_RESET);


void init_screen(){
  if(!display.begin(SSD1306_SWITCHCAPVCC, SCREEN_ADDRESS)) {
    Serial.println(F("SSD1306 allocation failed"));
  }
  display.clearDisplay();
  display.display();
}


void update_screen(){
  display.clearDisplay();
  display.setTextSize(1);
  display.setTextColor(SSD1306_WHITE);
  display.setCursor(0,0);
  // Row1.
  display.println(ip_address);
  // Row2.
  display.print(float(voltage_read[list_id[active_num_in_list]])/10);display.print(F("V"));
  display.print(F(" "));
  if(wifi_mode == 1){display.println(ap_ssid);}
  else if(wifi_mode == 2){display.println(sta_ssid);}
  else if(wifi_mode == 3){display.print(F("TRY:"));display.println(sta_ssid);}
  // Row3.
  display.print(F("WIFI:"));

  if(wifi_mode == 1){display.print(F(" AP "));display.println(ap_ssid);}
  else if(wifi_mode == 2){display.print(F(" STA "));display.println(sta_ssid);}
  else if(wifi_mode == 3){display.print(F(" TRY:"));display.print(sta_ssid);display.println(F(""));}

  // Row4.
  if(search_num){
    display.print(F("N:"));display.print(search_num);display.print(F(" ID:"));display.print(list_id[active_num_in_list]);
    display.print(F("-"));display.print(mode_read[list_id[active_num_in_list]]);
    display.print(F(" POS:"));display.println(pos_read[list_id[active_num_in_list]]);
  }
  else{
    display.println(F("No servo detected."));
  }
  display.display();
}


void ping_all(bool search_command){
  if(search_command){
    rgb_color(0, 255, 64);

    // Clean up old servo objects
    for(int i = 0; i < 253; i++){
      if(servos[i]){
        delete servos[i];
        servos[i] = nullptr;
      }
    }

    search_num = 0;
    searched_status = true;
    search_finished = false;
    xSemaphoreTake(servo_bus_mutex, portMAX_DELAY);
    for(int i = 0; i <= max_id; i++){
      // Either servo type works for ping
      servo_bus.set_servo_type(ServoBusApi::ServoType::STS);
      auto ping_result = servo_bus.ping(i);

      display.clearDisplay();
      display.setTextSize(1);
      display.setTextColor(SSD1306_WHITE);
      display.setCursor(0,0);
      display.println(F("Searching Servos..."));
      display.print(F("max_id "));display.print(max_id);
      display.print(F("-Ping:"));display.println(i);
      display.print(F("Detected:"));

      for(int j = 0; j < search_num; j++){
        display.print(list_id[j]);display.print(F(" "));
      }
      display.display();

      if(ping_result.has_value()){
        // Infer servo type (SC vs STS) and create typed object
        auto inferred_type = Servo::infer_servo_type(&servo_bus, i);
        if(inferred_type){
          if(*inferred_type == ServoBusApi::ServoType::SC){
            servos[i] = new SCServo(&servo_bus, i);
          } else {
            servos[i] = new STSServo(&servo_bus, i);
          }
        } else {
          // Fallback to configured default if inference fails
          Serial.printf("Servo %d: type inference failed, using default\n", i);
          if(servo_type_select == 1){
            servos[i] = new STSServo(&servo_bus, i);
          } else {
            servos[i] = new SCServo(&servo_bus, i);
          }
        }
        servos[i]->read_info();  // Load angle limits from EEPROM

        const char* type_str = (servos[i]->type() == ServoBusApi::ServoType::STS) ? "STS" : "SC";
        Serial.printf("Servo %d: %s (range %d-%d)\n", i, type_str,
                       servos[i]->min_encoder_angle(), servos[i]->max_encoder_angle());

        list_id[search_num] = i;
        search_num++;
      }
    }
    for(int i = 0; i < search_num; i++){
      Serial.print(list_id[i]);Serial.print(" ");
    }
    Serial.println();
    xSemaphoreGive(servo_bus_mutex);
    searched_status = false;
    search_finished = true;
    search_cmd      = false;
    rgb_off();
  }
}


void init_board_dev(){
    Wire.begin(S_SDA, S_SCL);
    init_screen();
    init_rgb();
}




void info_update_threading(void *pv_parameter){
  unsigned long last_screen = 0;
  while(1){
    // Handle rescan requests from the web UI
    if(search_cmd){
      ping_all(true);
    }

    // Poll telemetry for all detected servos
    for(int i = 0; i < search_num; i++){
      get_feedback(list_id[i]);
    }
    // Update screen/wifi at a slower rate (every 500ms)
    unsigned long now = millis();
    if(now - last_screen > 500){
      get_wifi_status();
      update_screen();
      last_screen = now;
    }
    delay(threading_interval);
  }
}


void client_threading(void *pv_parameter){
  while(1){
    server.handleClient();
    delay(client_interval);
  }
}


void init_threads(){
  xTaskCreatePinnedToCore(&info_update_threading, "InfoUpdate", 8192, NULL, 5, &screen_update_handle, ARDUINO_RUNNING_CORE);
  xTaskCreate(&client_threading, "Client", 8192, NULL, 5, &client_cmd_handle);
}