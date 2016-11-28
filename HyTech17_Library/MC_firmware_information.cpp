/*
  MC_firmware_information.cpp - RMS Inverter CAN message: Firmware Information
  Created by Nathan Cheek, November 23, 2016.
 */

#include "HyTech17.h"

MC_firmware_information::MC_firmware_information() {
}

MC_firmware_information::MC_firmware_information(uint8_t buf[8]) {
  load(buf);
}

void MC_firmware_information::load(uint8_t buf[8]) {
  memcpy(&message, buf, sizeof(CAN_message_mc_firmware_information_t));
}

int16_t MC_firmware_information::get_eeprom_version_project_code() {
  return message.eeprom_version_project_code;
}

int16_t MC_firmware_information::get_software_version() {
  return message.software_version;
}

uint32_t MC_firmware_information::get_date_code_mmdd() {
  return message.date_code_mmdd;
}

uint32_t MC_firmware_information::get_date_code_yyyy() {
  return message.date_code_yyyy;
}