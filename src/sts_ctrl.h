#pragma once
#include "sc_servo.h"
#include "sts_servo.h"
#include <math.h>

// Mutex protecting all servo_bus access (Serial1).
// Must be taken before any read/write to servo_bus or Servo* methods.
SemaphoreHandle_t servo_bus_mutex = nullptr;

// === Servo configuration ===
// Set to 1 for STS servos, 2 for SC servos
int servo_type_select = 2;    // 1=STS, 2=SC

// Derived constants (set in init_servo based on servo_type_select)
float servo_digital_range;
float servo_angle_range;
float servo_digital_middle;
int   servo_init_acc;
int   servo_max_speed;
int   max_speed_x;
int   servo_init_speed;

// Per-servo polymorphic pointers, allocated during scan.
// servos[id] is non-null for detected servos, null otherwise.
Servo* servos[253] = {nullptr};

// Fallback instances for servos not yet typed (before scan completes)
SCServo  sc_servo_fallback(&servo_bus, 0);
STSServo sts_servo_fallback(&servo_bus, 0);

// active_servo: default type for un-typed operations
Servo*   active_servo = nullptr;

// Get the Servo* for a given ID.
// Returns the typed servo from the servos[] array if available,
// otherwise falls back to the global default with the given ID.
inline Servo* servo_for_id(uint8_t id) {
  if (servos[id]) return servos[id];
  // Fallback for untyped servos (before scan or if inference failed)
  if (servo_type_select == 1) {
    sts_servo_fallback = STSServo(&servo_bus, id);
    return &sts_servo_fallback;
  } else {
    sc_servo_fallback = SCServo(&servo_bus, id);
    return &sc_servo_fallback;
  }
}


bool torque_list[253];

// Feedback buffers indexed by servo ID
int16_t  load_read[253];
int16_t  speed_read[253];
byte     voltage_read[253];
int      current_read[253];
int16_t  pos_read[253];
int16_t  goal_read[253];
int16_t  mode_read[253];
int16_t  temp_read[253];
uint8_t  alarm_read[253];

// Active servo list & search state
byte list_id[253];
byte search_num = 0;
bool searched_status = false;
bool search_finished = false;
bool search_cmd      = false;
byte active_num_in_list = 0;
int16_t active_servo_speed = 100;


// ----------- Core functions -----------

void write_servo_pos_ex(byte id, uint16_t position, uint16_t speed, uint8_t acc) {
  auto* s = servo_for_id(id);
  xSemaphoreTake(servo_bus_mutex, portMAX_DELAY);
  if (s->type() == ServoBusApi::ServoType::STS && acc > 0 && s->info_loaded()) {
    static_cast<STSServo*>(s)->move_to_encoder_angle_with_accel(position, speed, acc);
  } else {
    servo_bus.set_servo_type(s->type());
    servo_bus.write_position(id, position, 0, speed);
  }
  xSemaphoreGive(servo_bus_mutex);
}

void write_servo_pos(byte id, uint16_t position, uint16_t time_ms, uint16_t speed) {
  auto* s = servo_for_id(id);
  xSemaphoreTake(servo_bus_mutex, portMAX_DELAY);
  servo_bus.set_servo_type(s->type());
  servo_bus.write_position(id, position, time_ms, speed);
  xSemaphoreGive(servo_bus_mutex);
}


void get_feedback(byte servo_id) {
  auto* s = servo_for_id(servo_id);

  xSemaphoreTake(servo_bus_mutex, portMAX_DELAY);
  auto pos = s->read_encoder_angle();
  if (!pos) { xSemaphoreGive(servo_bus_mutex); return; }
  pos_read[servo_id] = *pos;

  auto goal = s->read_goal_position();
  if (goal) goal_read[servo_id] = *goal;

  auto spd = s->read_speed();
  if (spd) speed_read[servo_id] = *spd;

  auto load = s->read_load();
  if (load) load_read[servo_id] = *load;

  auto volt = s->read_voltage();
  if (volt) voltage_read[servo_id] = (byte)(*volt * 10);  // store as raw (tenths of volts)

  auto temp = s->read_temperature();
  if (temp) temp_read[servo_id] = (int16_t)*temp;

  auto cur = s->read_current();
  if (cur) current_read[servo_id] = *cur;

  auto mode = s->read_mode();
  if (mode) mode_read[servo_id] = *mode;

  auto torq = s->is_torque_enabled();
  if (torq) torque_list[servo_id] = *torq;

  auto alarm = s->read_alarm_status();
  if (alarm) alarm_read[servo_id] = *alarm;
  xSemaphoreGive(servo_bus_mutex);
}


void init_servo() {
  servo_bus_mutex = xSemaphoreCreateMutex();

  Serial1.begin(1000000, SERIAL_8N1, S_RXD, S_TXD);
  servo_bus.set_serial(&Serial1);
  servo_bus.set_echo_enabled(false);    // Waveshare board: HW TX/RX isolation
  while (!Serial1) {}

  // Configure type-specific constants
  if (servo_type_select == 1) {
    servo_digital_range  = 4095.0;
    servo_angle_range    = 360.0;
    servo_digital_middle = 2047.0;
    servo_init_acc       = 100;
    servo_max_speed      = 4000;
    max_speed_x         = 4000;
    servo_init_speed     = 2000;
    active_servo = &sts_servo_fallback;
    servo_bus.set_servo_type(ServoBusApi::ServoType::STS);
  } else {
    servo_digital_range  = 1023.0;
    servo_angle_range    = 210.0;
    servo_digital_middle = 511.0;
    servo_init_acc       = 0;
    servo_max_speed      = 1500;
    max_speed_x         = 1500;
    servo_init_speed     = 1500;
    active_servo = &sc_servo_fallback;
    servo_bus.set_servo_type(ServoBusApi::ServoType::SC);
  }

  for (int i = 0; i < 253; i++) {
    torque_list[i] = true;
  }
}


void set_middle(byte input_id) {
  if (servo_type_select == 1) {
    auto* s = servo_for_id(input_id);
    xSemaphoreTake(servo_bus_mutex, portMAX_DELAY);
    servo_bus.set_servo_type(s->type());
    servo_bus.set_offset(input_id, 0);
    xSemaphoreGive(servo_bus_mutex);
  }
}


void set_mode(byte input_id, byte input_mode) {
  auto* s = servo_for_id(input_id);
  xSemaphoreTake(servo_bus_mutex, portMAX_DELAY);
  if (input_mode == 0) {
    s->restore_position_mode();
  } else if (input_mode == 3) {
    s->enable_motor_mode();
  }
  xSemaphoreGive(servo_bus_mutex);
}


void set_servo_id(byte id_select, byte id_set) {
  if (id_set > max_id) { max_id = id_set; }
  xSemaphoreTake(servo_bus_mutex, portMAX_DELAY);
  servo_bus.set_servo_type(active_servo->type());
  servo_bus.set_servo_id_permanent(id_select, id_set);
  xSemaphoreGive(servo_bus_mutex);
}


void stop_servo(byte servo_id) {
  auto* s = servo_for_id(servo_id);
  xSemaphoreTake(servo_bus_mutex, portMAX_DELAY);
  s->disable_torque();
  delay(10);
  s->enable_torque();
  xSemaphoreGive(servo_bus_mutex);
}


void set_servo_torque(byte servo_id, uint8_t enable_cmd) {
  auto* s = servo_for_id(servo_id);
  xSemaphoreTake(servo_bus_mutex, portMAX_DELAY);
  if (enable_cmd) {
    s->enable_torque();
  } else {
    s->disable_torque();
  }
  xSemaphoreGive(servo_bus_mutex);
}