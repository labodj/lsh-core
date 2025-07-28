#ifndef J2_CONFIG_HPP
#define J2_CONFIG_HPP

// --- 1. CONTRACT: Specify which hardware library to use ---
#define LSH_HARDWARE_INCLUDE <Controllino.h>

// --- 2. CONTRACT: Define the constants for this build ---
#define LSH_DEVICE_NAME "j2"
#define LSH_MAX_CLICKABLES 10
#define LSH_MAX_ACTUATORS 9
#define LSH_MAX_INDICATORS 1
#define LSH_COM_SERIAL &Serial2
#define LSH_DEBUG_SERIAL &Serial

#endif // J2_CONFIG_HPP