#pragma once
#include <WiFi.h>
#include <WebServer.h>
#include "web_page.h"


WebServer server(80);


void handle_root() {
  server.send(200, "text/html", index_html);
}


void init_web_ctrl_server(){
  server.on("/", handle_root);

  // === JSON API endpoints ===

  server.on("/api/scan", [](){
    String json = "{\"servos\":[";
    for(int i = 0; i < search_num; i++){
      if(i > 0) json += ",";
      uint8_t id = list_id[i];
      Servo* s = servos[id];
      bool is_sts = s && (s->type() == ServoBusApi::ServoType::STS);
      json += "{\"id\":" + String(id);
      json += ",\"type\":\"" + String(is_sts ? "STS" : "SC") + "\"";
      json += ",\"range\":" + String(s ? s->full_range() : (int)servo_digital_range);
      json += ",\"middle\":" + String(s ? s->full_range() / 2 : (int)servo_digital_middle);
      json += ",\"hasCurrent\":" + String(s && s->current_supported() ? "true" : "false");
      String sname = get_servo_name(id);
      json += ",\"name\":\"";
      // Escape quotes in name for JSON safety
      for (unsigned int ci = 0; ci < sname.length(); ci++) {
        char ch = sname.charAt(ci);
        if (ch == '"' || ch == '\\') json += '\\';
        json += ch;
      }
      json += "\"";
      json += "}";
    }
    json += "]}";
    server.send(200, "application/json", json);
  });

  server.on("/api/status_all", [](){
    String json = "{\"servos\":[";
    for(int i = 0; i < search_num; i++){
      if(i > 0) json += ",";
      uint8_t id = list_id[i];
      Servo* s = servos[id];
      bool is_sts = s && (s->type() == ServoBusApi::ServoType::STS);
      json += "{\"id\":" + String(id);
      json += ",\"type\":\"" + String(is_sts ? "STS" : "SC") + "\"";
      json += ",\"pos\":" + String(pos_read[id]);
      json += ",\"goal\":" + String(goal_read[id]);
      json += ",\"speed\":" + String(speed_read[id]);
      json += ",\"load\":" + String(load_read[id]);
      json += ",\"voltage\":" + String(float(voltage_read[id]) / 10.0, 1);
      json += ",\"temp\":" + String(temp_read[id]);
      json += ",\"current\":" + String(current_read[id]);
      json += ",\"mode\":" + String(mode_read[id]);
      json += ",\"torque\":" + String(torque_list[id] ? "true" : "false");
      json += ",\"range\":" + String(s ? s->full_range() : (int)servo_digital_range);
      json += ",\"alarm\":" + String(alarm_read[id]);
      json += "}";
    }
    json += "]}";
    server.send(200, "application/json", json);
  });

  server.on("/api/setpos", [](){
    if(!server.hasArg("id") || !server.hasArg("pos")){
      server.send(400, "application/json", "{\"error\":\"missing id or pos\"}");
      return;
    }
    int id = server.arg("id").toInt();
    int pos = server.arg("pos").toInt();
    int speed = server.hasArg("speed") ? server.arg("speed").toInt() : active_servo_speed;
    int acc = server.hasArg("acc") ? server.arg("acc").toInt() : servo_init_acc;

    if(id < 0 || id > 252 || !servos[id]){
      server.send(404, "application/json", "{\"error\":\"servo not found\"}");
      return;
    }

    int range = servos[id]->full_range();
    pos = constrain(pos, 0, range);
    speed = constrain(speed, 1, (int)servo_max_speed);
    acc = constrain(acc, 0, 255);

    write_servo_pos_ex(id, pos, speed, acc);
    server.send(200, "application/json", "{\"ok\":true}");
  });

  server.on("/api/rescan", [](){
    search_cmd = true;
    server.send(200, "application/json", "{\"ok\":true,\"message\":\"scan started\"}");
  });

  server.on("/api/scan_status", [](){
    String json = "{\"scanning\":";
    json += searched_status ? "true" : "false";
    json += ",\"finished\":";
    json += search_finished ? "true" : "false";
    json += "}";
    server.send(200, "application/json", json);
  });

  server.on("/api/stop", [](){
    if(!server.hasArg("id")){
      server.send(400, "application/json", "{\"error\":\"missing id\"}");
      return;
    }
    int id = server.arg("id").toInt();
    if(id < 0 || id > 252 || !servos[id]){
      server.send(404, "application/json", "{\"error\":\"servo not found\"}");
      return;
    }
    int mode = mode_read[id];
    if(mode == 3) {
      xSemaphoreTake(servo_bus_mutex, portMAX_DELAY);
      servo_for_id(id)->set_motor_speed(0);
      xSemaphoreGive(servo_bus_mutex);
    } else {
      stop_servo(id);
    }
    server.send(200, "application/json", "{\"ok\":true}");
  });

  server.on("/api/torque", [](){
    if(!server.hasArg("id") || !server.hasArg("enable")){
      server.send(400, "application/json", "{\"error\":\"missing id or enable\"}");
      return;
    }
    int id = server.arg("id").toInt();
    int enable = server.arg("enable").toInt();

    if(id < 0 || id > 252 || !servos[id]){
      server.send(404, "application/json", "{\"error\":\"servo not found\"}");
      return;
    }

    set_servo_torque(id, enable);
    torque_list[id] = (enable != 0);
    server.send(200, "application/json", "{\"ok\":true}");
  });

  server.on("/api/torque_limit", [](){
    if(!server.hasArg("id")){
      server.send(400, "application/json", "{\"error\":\"missing id\"}");
      return;
    }
    int id = server.arg("id").toInt();
    if(id < 0 || id > 252 || !servos[id]){
      server.send(404, "application/json", "{\"error\":\"servo not found\"}");
      return;
    }
    if(servos[id]->type() != ServoBusApi::ServoType::STS){
      server.send(400, "application/json", "{\"error\":\"torque limit only supported on STS servos\"}");
      return;
    }
    auto* sts = static_cast<STSServo*>(servos[id]);
    if(server.hasArg("value")){
      int val = server.arg("value").toInt();
      if(val < 0) val = 0;
      if(val > 1023) val = 1023;
      xSemaphoreTake(servo_bus_mutex, portMAX_DELAY);
      sts->set_torque_limit((uint16_t)val);
      xSemaphoreGive(servo_bus_mutex);
      server.send(200, "application/json", "{\"ok\":true}");
    } else {
      xSemaphoreTake(servo_bus_mutex, portMAX_DELAY);
      uint16_t val = sts->read_torque_limit();
      xSemaphoreGive(servo_bus_mutex);
      server.send(200, "application/json", "{\"value\":" + String(val) + "}");
    }
  });

  server.on("/api/angle_limits", [](){
    if(!server.hasArg("id")){
      server.send(400, "application/json", "{\"error\":\"missing id\"}");
      return;
    }
    int id = server.arg("id").toInt();
    if(id < 0 || id > 252 || !servos[id]){
      server.send(404, "application/json", "{\"error\":\"servo not found\"}");
      return;
    }
    if(server.hasArg("min") && server.hasArg("max")){
      int minVal = server.arg("min").toInt();
      int maxVal = server.arg("max").toInt();
      int range = servos[id]->full_range();
      if(minVal < 0 || maxVal < 0 || minVal > range || maxVal > range || minVal >= maxVal){
        server.send(400, "application/json", "{\"error\":\"invalid angle limits\"}");
        return;
      }
      xSemaphoreTake(servo_bus_mutex, portMAX_DELAY);
      bool ok = servos[id]->write_angle_limits((uint16_t)minVal, (uint16_t)maxVal);
      xSemaphoreGive(servo_bus_mutex);
      if(ok){
        server.send(200, "application/json", "{\"ok\":true}");
      } else {
        server.send(500, "application/json", "{\"error\":\"write failed\"}");
      }
    } else {
      xSemaphoreTake(servo_bus_mutex, portMAX_DELAY);
      auto limits = servos[id]->read_angle_limits();
      xSemaphoreGive(servo_bus_mutex);
      if(limits){
        String json = "{\"min\":" + String(limits->min_angle) + ",\"max\":" + String(limits->max_angle) + "}";
        server.send(200, "application/json", json);
      } else {
        server.send(500, "application/json", "{\"error\":\"read failed\"}");
      }
    }
  });

  server.on("/api/set_id", [](){
    if(!server.hasArg("id") || !server.hasArg("new_id")){
      server.send(400, "application/json", "{\"error\":\"missing id or new_id\"}");
      return;
    }
    int id = server.arg("id").toInt();
    int new_id = server.arg("new_id").toInt();
    if(id < 0 || id > 252 || !servos[id]){
      server.send(404, "application/json", "{\"error\":\"servo not found\"}");
      return;
    }
    if(new_id < 0 || new_id > 252){
      server.send(400, "application/json", "{\"error\":\"new_id out of range\"}");
      return;
    }
    xSemaphoreTake(servo_bus_mutex, portMAX_DELAY);
    bool ok = servos[id]->set_id((uint8_t)new_id);
    xSemaphoreGive(servo_bus_mutex);
    if(ok){
      // Move servo pointer to new ID slot
      servos[new_id] = servos[id];
      servos[id] = nullptr;
      // Migrate servo name to new ID
      migrate_servo_name((uint8_t)id, (uint8_t)new_id);
      // Update list_id
      for(int i = 0; i < search_num; i++){
        if(list_id[i] == id){ list_id[i] = new_id; break; }
      }
      server.send(200, "application/json", "{\"ok\":true,\"new_id\":" + String(new_id) + "}");
    } else {
      server.send(500, "application/json", "{\"error\":\"set_id failed\"}");
    }
  });

  server.on("/api/tuning", [](){
    if(!server.hasArg("id")){
      server.send(400, "application/json", "{\"error\":\"missing id\"}");
      return;
    }
    int id = server.arg("id").toInt();
    if(id < 0 || id > 252 || !servos[id]){
      server.send(404, "application/json", "{\"error\":\"servo not found\"}");
      return;
    }
    bool is_sts = servos[id]->type() == ServoBusApi::ServoType::STS;
    if(server.hasArg("p")){
      bool ok = false;
      if(is_sts){
        auto* sts = static_cast<STSServo*>(servos[id]);
        STSServo::TuningConfig cfg;
        cfg.p_coefficient = constrain(server.arg("p").toInt(), 0, 254);
        cfg.d_coefficient = constrain(server.arg("d").toInt(), 0, 254);
        cfg.i_coefficient = constrain(server.arg("i").toInt(), 0, 254);
        cfg.min_starting_force = constrain(server.arg("min_start").toInt(), 0, 1000);
        cfg.cw_dead = constrain(server.arg("cw_dead").toInt(), 0, 32);
        cfg.ccw_dead = constrain(server.arg("ccw_dead").toInt(), 0, 32);
        xSemaphoreTake(servo_bus_mutex, portMAX_DELAY);
        ok = sts->write_tuning_config(cfg);
        xSemaphoreGive(servo_bus_mutex);
      } else {
        auto* sc = static_cast<SCServo*>(servos[id]);
        SCServo::TuningConfig cfg;
        cfg.p_coefficient = constrain(server.arg("p").toInt(), 0, 254);
        cfg.d_coefficient = constrain(server.arg("d").toInt(), 0, 254);
        cfg.i_coefficient = constrain(server.arg("i").toInt(), 0, 254);
        cfg.min_starting_force = constrain(server.arg("min_start").toInt(), 0, 1000);
        cfg.cw_dead = constrain(server.arg("cw_dead").toInt(), 0, 32);
        cfg.ccw_dead = constrain(server.arg("ccw_dead").toInt(), 0, 32);
        cfg.hysteresis = constrain(server.arg("hysteresis").toInt(), 0, 32);
        xSemaphoreTake(servo_bus_mutex, portMAX_DELAY);
        ok = sc->write_tuning_config(cfg);
        xSemaphoreGive(servo_bus_mutex);
      }
      if(ok) server.send(200, "application/json", "{\"ok\":true}");
      else server.send(500, "application/json", "{\"error\":\"write failed\"}");
    } else {
      String json = "{";
      if(is_sts){
        auto* sts = static_cast<STSServo*>(servos[id]);
        xSemaphoreTake(servo_bus_mutex, portMAX_DELAY);
        auto cfg = sts->read_tuning_config();
        xSemaphoreGive(servo_bus_mutex);
        if(!cfg){ server.send(500, "application/json", "{\"error\":\"read failed\"}"); return; }
        json += "\"p\":" + String(cfg->p_coefficient);
        json += ",\"d\":" + String(cfg->d_coefficient);
        json += ",\"i\":" + String(cfg->i_coefficient);
        json += ",\"min_start\":" + String(cfg->min_starting_force);
        json += ",\"cw_dead\":" + String(cfg->cw_dead);
        json += ",\"ccw_dead\":" + String(cfg->ccw_dead);
      } else {
        auto* sc = static_cast<SCServo*>(servos[id]);
        xSemaphoreTake(servo_bus_mutex, portMAX_DELAY);
        auto cfg = sc->read_tuning_config();
        xSemaphoreGive(servo_bus_mutex);
        if(!cfg){ server.send(500, "application/json", "{\"error\":\"read failed\"}"); return; }
        json += "\"p\":" + String(cfg->p_coefficient);
        json += ",\"d\":" + String(cfg->d_coefficient);
        json += ",\"i\":" + String(cfg->i_coefficient);
        json += ",\"min_start\":" + String(cfg->min_starting_force);
        json += ",\"cw_dead\":" + String(cfg->cw_dead);
        json += ",\"ccw_dead\":" + String(cfg->ccw_dead);
        json += ",\"hysteresis\":" + String(cfg->hysteresis);
      }
      json += "}";
      server.send(200, "application/json", json);
    }
  });

  server.on("/api/safety", [](){
    if(!server.hasArg("id")){
      server.send(400, "application/json", "{\"error\":\"missing id\"}");
      return;
    }
    int id = server.arg("id").toInt();
    if(id < 0 || id > 252 || !servos[id]){
      server.send(404, "application/json", "{\"error\":\"servo not found\"}");
      return;
    }
    bool is_sts = servos[id]->type() == ServoBusApi::ServoType::STS;
    if(server.hasArg("max_temp")){
      bool ok = false;
      if(is_sts){
        auto* sts = static_cast<STSServo*>(servos[id]);
        STSServo::SafetyConfig cfg;
        cfg.max_temp = constrain(server.arg("max_temp").toInt(), 0, 255);
        cfg.max_voltage = constrain(server.arg("max_voltage").toInt(), 0, 255);
        cfg.min_voltage = constrain(server.arg("min_voltage").toInt(), 0, 255);
        cfg.max_torque = constrain(server.arg("max_torque").toInt(), 0, 1000);
        cfg.protection_current = constrain(server.arg("prot_current").toInt(), 0, 511);
        cfg.protective_torque = constrain(server.arg("prot_torque").toInt(), 0, 100);
        cfg.protection_time = constrain(server.arg("prot_time").toInt(), 0, 254);
        cfg.overload_torque = constrain(server.arg("overload_torque").toInt(), 0, 100);
        cfg.overcurrent_prot_time = constrain(server.arg("overcurrent_time").toInt(), 0, 254);
        cfg.unload_conditions = constrain(server.arg("unload").toInt(), 0, 63);
        cfg.led_alarm_conditions = constrain(server.arg("led_alarm").toInt(), 0, 63);
        xSemaphoreTake(servo_bus_mutex, portMAX_DELAY);
        ok = sts->write_safety_config(cfg);
        xSemaphoreGive(servo_bus_mutex);
      } else {
        auto* sc = static_cast<SCServo*>(servos[id]);
        SCServo::SafetyConfig cfg;
        cfg.max_temp = constrain(server.arg("max_temp").toInt(), 0, 100);
        cfg.max_voltage = constrain(server.arg("max_voltage").toInt(), 0, 254);
        cfg.min_voltage = constrain(server.arg("min_voltage").toInt(), 0, 254);
        cfg.max_torque = constrain(server.arg("max_torque").toInt(), 0, 1000);
        cfg.protective_torque = constrain(server.arg("prot_torque").toInt(), 0, 100);
        cfg.protection_time = constrain(server.arg("prot_time").toInt(), 0, 254);
        cfg.overload_torque = constrain(server.arg("overload_torque").toInt(), 0, 100);
        cfg.unload_conditions = constrain(server.arg("unload").toInt(), 0, 37);
        cfg.led_alarm_conditions = constrain(server.arg("led_alarm").toInt(), 0, 37);
        xSemaphoreTake(servo_bus_mutex, portMAX_DELAY);
        ok = sc->write_safety_config(cfg);
        xSemaphoreGive(servo_bus_mutex);
      }
      if(ok) server.send(200, "application/json", "{\"ok\":true}");
      else server.send(500, "application/json", "{\"error\":\"write failed\"}");
    } else {
      String json = "{";
      if(is_sts){
        auto* sts = static_cast<STSServo*>(servos[id]);
        xSemaphoreTake(servo_bus_mutex, portMAX_DELAY);
        auto cfg = sts->read_safety_config();
        xSemaphoreGive(servo_bus_mutex);
        if(!cfg){ server.send(500, "application/json", "{\"error\":\"read failed\"}"); return; }
        json += "\"max_temp\":" + String(cfg->max_temp);
        json += ",\"max_voltage\":" + String(cfg->max_voltage);
        json += ",\"min_voltage\":" + String(cfg->min_voltage);
        json += ",\"max_torque\":" + String(cfg->max_torque);
        json += ",\"prot_current\":" + String(cfg->protection_current);
        json += ",\"prot_torque\":" + String(cfg->protective_torque);
        json += ",\"prot_time\":" + String(cfg->protection_time);
        json += ",\"overload_torque\":" + String(cfg->overload_torque);
        json += ",\"overcurrent_time\":" + String(cfg->overcurrent_prot_time);
        json += ",\"unload\":" + String(cfg->unload_conditions);
        json += ",\"led_alarm\":" + String(cfg->led_alarm_conditions);
      } else {
        auto* sc = static_cast<SCServo*>(servos[id]);
        xSemaphoreTake(servo_bus_mutex, portMAX_DELAY);
        auto cfg = sc->read_safety_config();
        xSemaphoreGive(servo_bus_mutex);
        if(!cfg){ server.send(500, "application/json", "{\"error\":\"read failed\"}"); return; }
        json += "\"max_temp\":" + String(cfg->max_temp);
        json += ",\"max_voltage\":" + String(cfg->max_voltage);
        json += ",\"min_voltage\":" + String(cfg->min_voltage);
        json += ",\"max_torque\":" + String(cfg->max_torque);
        json += ",\"prot_torque\":" + String(cfg->protective_torque);
        json += ",\"prot_time\":" + String(cfg->protection_time);
        json += ",\"overload_torque\":" + String(cfg->overload_torque);
        json += ",\"unload\":" + String(cfg->unload_conditions);
        json += ",\"led_alarm\":" + String(cfg->led_alarm_conditions);
      }
      json += "}";
      server.send(200, "application/json", json);
    }
  });

  server.on("/api/set_name", [](){
    if(!server.hasArg("id")){
      server.send(400, "application/json", "{\"error\":\"missing id\"}");
      return;
    }
    int id = server.arg("id").toInt();
    if(id < 0 || id > 252){
      server.send(400, "application/json", "{\"error\":\"id out of range\"}");
      return;
    }
    if(server.hasArg("name")){
      String name = server.arg("name");
      if(name.length() > 20) name = name.substring(0, 20);
      set_servo_name((uint8_t)id, name);
      server.send(200, "application/json", "{\"ok\":true}");
    } else {
      String name = get_servo_name((uint8_t)id);
      server.send(200, "application/json", "{\"name\":\"" + name + "\"}");
    }
  });

  // Start server
  server.begin();
  Serial.println("Server Starts.");
}


void init_web_server(){
  init_web_ctrl_server();
}


void get_mac(){
  WiFi.mode(WIFI_AP_STA);
  mac_address = WiFi.macAddress();
  Serial.print("MAC:");
  Serial.println(WiFi.macAddress());
}


void get_ip(){
  ip_address = WiFi.localIP();
}


void start_ap(){
  WiFi.softAP(ap_ssid, ap_pwd);
  IPAddress my_ip = WiFi.softAPIP();
  ip_address = my_ip;
  Serial.print("AP IP address: ");
  Serial.println(my_ip);
  wifi_mode = 1;
}


void start_sta(){
  wifi_mode = 3;
  Serial.print("Connecting to ");
  Serial.println(sta_ssid);
  WiFi.begin(sta_ssid, sta_pwd);

  // Wait up to 10 seconds for connection.
  int attempts = 0;
  while (WiFi.status() != WL_CONNECTED && attempts < 20) {
    delay(500);
    Serial.print(".");
    attempts++;
  }
  Serial.println();

  if (WiFi.status() == WL_CONNECTED) {
    wifi_mode = 2;
    get_ip();
    Serial.print("Connected to ");
    Serial.println(sta_ssid);
    Serial.print("IP address: ");
    Serial.println(ip_address);
  } else {
    Serial.println("STA connection failed, falling back to AP mode.");
    WiFi.disconnect();
    start_ap();
  }
}


void get_wifi_status(){
  if(WiFi.status() == WL_CONNECTED){
    wifi_mode = 2;
    get_ip();
    wifi_rssi = WiFi.RSSI();
  }
  else if(WiFi.status() == WL_CONNECTION_LOST && wifi_mode == 2){
    wifi_mode = 3;
    WiFi.reconnect();
  }
}


void init_wifi(){
  start_sta();
}


