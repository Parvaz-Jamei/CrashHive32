#pragma once

#include <stdint.h>

#include "CH32BlackBox.h"
#include "CH32Config.h"

class CH32BootState {
 public:
  CH32BootState();
  void begin(CH32BlackBox* blackBox);
  bool tick();
  bool isBootStorm() const;
  uint8_t getBootStreak() const;
  void markBootStable();

 private:
  bool isValid() const;
  void reset();
  void updateCrc();
  uint8_t computeCrc() const;

  CH32BlackBox* blackBox_;
  uint32_t bootMs_;
};
