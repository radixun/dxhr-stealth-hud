#pragma once
#include <windows.h>
#include <cstdint>
// Runtime observation only. No achievement flags or save data are changed.
namespace StealthEvents {
struct Sample { bool available; LONG detections, alarms, restores; };
bool Install(uintptr_t base);
Sample Read();
}
