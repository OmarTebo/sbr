#pragma once
// Use MPU6050 (definite)
#define USE_MPU6050

// Control loop
#define CONTROL_LOOP_HZ 200
#define CONTROL_LOOP_DT_S (1.0f / CONTROL_LOOP_HZ)

// PID safe limits (angular velocity output, deg/s)
#define PID_OUTPUT_MIN_F -1000.0f
#define PID_OUTPUT_MAX_F 1000.0f

// PID defaults (used only if no stored values exist)
#define DEFAULT_PID_KP 1.0f
#define DEFAULT_PID_KI 0.0f
#define DEFAULT_PID_KD 1.0f

// NVS namespace and keys
#define PREFS_NAMESPACE "sbr2"   // NVS namespace
#define PREFS_KEY_KP   "kp"
#define PREFS_KEY_KI   "ki"
#define PREFS_KEY_KD   "kd"

// IMU Calibration constants
// Magic number: 0xCA1B1234
// Origin: Chosen to verify valid calibration data in NVS
// Rationale: Unique identifier to detect corrupted or uninitialized calibration data
#define IMU_CALIB_MAGIC 0xCA1B1234

// Magic number: 1
// Origin: Version number for calibration data format
// Rationale: Allows future format changes while maintaining backward compatibility
#define IMU_CALIB_VERSION 1

// Magic number: 200
// Origin: Number of samples for calibration averaging
// Rationale: 200 samples at 5ms delay = 1 second of averaging for stable calibration
// Reference: Typical IMU calibration uses 1-2 seconds of data
#define IMU_CALIB_SAMPLES 200

// Magic number: 5
// Origin: Delay between calibration samples in milliseconds
// Rationale: 5ms delay allows sensor to settle between readings, prevents aliasing
// Reference: MPU6050 sample rate considerations
#define IMU_CALIB_DELAY_MS 5

// NVS keys for calibration persistence
#define PREFS_KEY_CALIB_PITCH_OFFSET "cal_pitch"
#define PREFS_KEY_CALIB_ROLL_OFFSET  "cal_roll"
#define PREFS_KEY_CALIB_MAGIC        "cal_magic"
#define PREFS_KEY_CALIB_VERSION      "cal_ver"

// IMU DLPF persistence (sample-rate configuration)
// Magic number: 0xD1PF1234
// Origin: Distinct identifier for stored DLPF config in NVS
// Rationale: Allows robust detection of valid DLPF configuration separate from
//            calibration data; ensures first-boot uses library defaults until
//            user explicitly tunes and saves DLPF via serial commands.
#define IMU_DLPF_MAGIC 0xD1PF1234
// NVS keys for DLPF runtime configuration
#define PREFS_KEY_IMU_DLPF_MAGIC "dlpf_magic"
#define PREFS_KEY_IMU_DLPF_CFG   "dlpf_cfg"
#define PREFS_KEY_IMU_SMPLRT     "dlpf_div"

// IMU stall detection thresholds
#define IMU_STALL_ANGLE_CHANGE_MIN 1e-5f  // Minimum angle change to detect frozen readings
#define IMU_STALL_TIMEOUT_MS 200          // Timeout in ms to consider IMU stalled

// Steps mapping (tune later)
#define STEPS_PER_DEGREE (3200.0f/360.0f) // ≈ 8.8888889

// I2C pins (from KiCad nets)
#define I2C_SDA_PIN 21
#define I2C_SCL_PIN 22
#define I2C_CLOCK_HZ 100000UL

// Motor pins (both motors rotate around x-axis, using roll for control)
#define LEFT_STEP_PIN 4
#define LEFT_DIR_PIN 0
#define RIGHT_STEP_PIN 2
#define RIGHT_DIR_PIN 15
#define LEFT_EN_PIN -1
#define RIGHT_EN_PIN -1

// Motor sign configuration: adjust if a motor spins the opposite direction
// These can be changed in software for testing without rewiring hardware
#define LEFT_MOTOR_SIGN 1
#define RIGHT_MOTOR_SIGN -1

// Motor swap/invert flags: set to true to swap left/right motors or invert individually
// Useful for testing without hardware changes
#define SWAP_MOTORS false  // If true, left motor uses right pins and vice versa
#define INVERT_LEFT_MOTOR false  // If true, invert left motor direction sign
#define INVERT_RIGHT_MOTOR false  // If true, invert right motor direction sign

// Motor driver defaults
#define MOTOR_DEFAULT_ACCELERATION 1000.0f  // steps/s^2
#define MOTOR_DEFAULT_MAX_SPEED 1000.0f     // steps/s

// Display pins (LED Matrix)
#define DISPLAY_DIN_PIN 23
#define DISPLAY_CLK_PIN 18
#define DISPLAY_CS1_PIN 17
#define DISPLAY_CS2_PIN 16

// Test mode configuration
// Magic number: false
// Origin: Compile-time safety flag
// Rationale: When true, motors are completely disabled at compile time for safe testing
#define TEST_MODE_ENABLED false  // Compile-time test mode (disables motor control)

// Magic number: false
// Origin: Runtime test mode flag
// Rationale: Allows toggling test mode via serial command for flexible testing
#define TEST_MODE_RUNTIME false  // Runtime test mode flag (can be toggled via serial)

// BLE configuration
// Magic number: true
// Origin: Enable BLE for remote PID tuning and tank control
// Rationale: Set to true to enable BLE for remote PID tuning and tank control
#define BLE_ENABLED true

// Boot tag for firmware identification
// Format: "pcb-v2-prototype-N" where N increments with each code change during prototyping
// Origin: PCB v2 prototyping phase identifier
// Rationale: Allows smoke test scripts to verify correct firmware is running and track prototype iterations
// Usage: Printed on boot and returned by GET_BOOT_TAG command
// IMPORTANT: Increment the number (N) with each significant code change during prototyping phase
#define BOOT_TAG "pcb-v2-prototype-1"

// Misc
#define SERIAL_BAUD 115200


