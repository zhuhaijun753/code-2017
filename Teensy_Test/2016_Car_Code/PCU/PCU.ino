/**
 * Nathan Cheek
 * 2016-11-18
 * Control Shutdown Circuit initialization.
 */
#include <FlexCAN.h>
#include <HyTech16.h>
#include <Metro.h>
#include <string.h>

/*
 * Pin definitions
 */
#define SENSE_BMS A0
#define SENSE_BRAKE A3
#define SENSE_IMD A1
#define SENSE_SHUTDOWN_OUT A2
#define SENSE_TEMP A4
#define SSR_BRAKE_LIGHT 13
#define SSR_LATCH 10
#define SSR_SOFTWARE_SHUTOFF 11

/*
 * Timers
 */
Metro timer_bms_faulting = Metro(500); // At startup the BMS DISCHARGE_OK line drops shortly
Metro timer_imd_faulting = Metro(500); // At startup the IMD OKHS line drops shortly
Metro timer_latch = Metro(1000);
Metro timer_state_send = Metro(100);

/*
 * Global variables
 */
boolean bms_fault = false;
boolean bms_faulting = false;
uint8_t btn_start_id = 0; // increments to differentiate separate button presses
uint8_t btn_start_new = 0;boolean imd_fault = false;
boolean imd_faulting = false;
uint8_t state = PCU_STATE_WAITING_BMS_IMD;

FlexCAN CAN(500000);
static CAN_message_t msg;

void setup() {
  pinMode(SSR_BRAKE_LIGHT, OUTPUT);
  pinMode(SSR_LATCH, OUTPUT);
  pinMode(SSR_SOFTWARE_SHUTOFF, OUTPUT);

  Serial.begin(115200);
  CAN.begin();
  delay(100);
  Serial.println("CAN transceiver initialized");
}

void loop() {
  /*
   * Handle incoming CAN messages
   */
  while (CAN.read(msg)) {
    if (msg.id == ID_TCU_STATUS) {
      CAN_message_tcu_status_t tcu_status;
      memcpy(msg.buf, &tcu_status, sizeof(tcu_status));
      if (btn_start_id != tcu_status.btn_start_id) {
        btn_start_id = tcu_status.btn_start_id;
        Serial.print("Start button pressed id ");
        Serial.println(btn_start_id);
      }
    }
  }

  /*
   * Send state over CAN
   */
  if (timer_state_send.check()) {
    CAN_message_pcu_status_t pcu_status = {state, bms_fault, imd_fault};
    memcpy(&pcu_status, msg.buf, sizeof(pcu_status));
    msg.id = ID_PCU_STATUS;
    msg.len = sizeof(pcu_status);
    CAN.write(msg);
  }

  switch (state) {
    case PCU_STATE_WAITING_BMS_IMD:
    if (analogRead(SENSE_IMD) > 100 && analogRead(SENSE_BMS) > 100) { // Wait till IMD and BMS signals go high at startup
      set_state(PCU_STATE_WAITING_DRIVER);
    }
    break;

    case PCU_STATE_WAITING_DRIVER:
    if (btn_start_new == btn_start_id) { // Start button has been pressed
      set_state(PCU_STATE_LATCHING);
    }
    break;

    case PCU_STATE_LATCHING:
    if (timer_latch.check()) { // Disable latching SSR
      set_state(PCU_STATE_SHUTDOWN_CIRCUIT_INITIALIZED);
    }
    break;

    case PCU_STATE_SHUTDOWN_CIRCUIT_INITIALIZED:
    break;

    case PCU_STATE_FATAL_FAULT:
    break;
  }

  /*
   * Start BMS fault timer if signal drops momentarily
   */
  if (state != PCU_STATE_WAITING_BMS_IMD && analogRead(SENSE_BMS) <= 50) {
    bms_faulting = true;
    timer_bms_faulting.reset();
  }

  /*
   * Reset BMS fault condition if signal comes back within timer period
   */
  if (bms_faulting && analogRead(SENSE_BMS) > 50) {
    bms_faulting = false;
  }

  /*
   * Declare BMS fault if signal still dropped
   */
  if (bms_faulting && timer_imd_faulting.check()) {
    bms_fault = true;
    set_state(PCU_STATE_FATAL_FAULT);
    digitalWrite(SSR_SOFTWARE_SHUTOFF, LOW);
    Serial.println("BMS fault detected");
  }

  /*
   * Start IMD fault timer if signal drops momentarily
   */
  if (state != PCU_STATE_WAITING_BMS_IMD && analogRead(SENSE_IMD) <= 50) {
    imd_faulting = true;
    timer_imd_faulting.reset();
  }

  /*
   * Reset IMD fault condition if signal comes back within timer period
   */
  if (imd_faulting && analogRead(SENSE_IMD) > 50) {
    imd_faulting = false;
  }

  /*
   * Declare IMD fault if signal still dropped
   */
  if (imd_faulting && timer_imd_faulting.check()) {
    imd_fault = true;
    set_state(PCU_STATE_FATAL_FAULT);
    digitalWrite(SSR_SOFTWARE_SHUTOFF, LOW);
    Serial.println("IMD fault detected");
  }
}

/*
 * Handle changes in state
 */
void set_state(uint8_t new_state) {
  if (state == new_state) {
    return;
  }
  state = new_state;
  if (new_state == PCU_STATE_WAITING_DRIVER) {
    btn_start_new = btn_start_id + 1;
  }
  if (new_state == PCU_STATE_LATCHING) {
    timer_latch.reset();
    digitalWrite(SSR_LATCH, HIGH);
    digitalWrite(SSR_SOFTWARE_SHUTOFF, HIGH);
    Serial.println("Latching");
  }
  if (new_state == PCU_STATE_SHUTDOWN_CIRCUIT_INITIALIZED) {
    digitalWrite(SSR_LATCH, LOW);
  }
}
