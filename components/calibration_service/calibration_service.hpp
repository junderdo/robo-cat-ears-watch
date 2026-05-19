/*
 * Description: Lighting service for Robo cat ears controller
 * Author: Jeff Underdown (junderdo)
 * Copyright (C) 2026 Milk Lab Creations
 * SPDX-License-Identifier: GPL-3.0-or-later
 */
#pragma once

#include <string>
#include <vector>
#include <cstdint>
#include <functional>

namespace robo_cat_ears {

/**
 * @brief Calibration data structure
 */
struct CalibrationData {
    int16_t left_azi;
    int16_t left_lat;
    int16_t right_azi;
    int16_t right_lat;
    
    CalibrationData() : left_azi(0), left_lat(0), right_azi(0), right_lat(0) {}
    
    /**
     * @brief Pack calibration data into binary format for efficient BLE transmission
     * 
     * Format: [left_azi:2bytes][left_lat:2bytes][right_azi:2bytes][right_lat:2bytes]
     * Max size: 2 + 2 + 2 + 2 = 8 bytes
     * 
     * @return Packed binary string
     */
    std::string pack() const;
    
    /**
     * @brief Unpack binary data into CalibrationData structure
     * 
     * @param packed Binary packed string
     * @param data Output CalibrationData structure
     * @return true if unpacking successful, false otherwise
     */
    static bool unpack(const std::string &packed, CalibrationData &data);
};

/**
 * @brief Calibration service class
 * 
 * Manages calibration data and communicates with the Bluetooth characteristic ABF1 using binary packed format
 */
class CalibrationService {
public:
    using DataLoadedCallback = std::function<void(const CalibrationData &data)>;
    
    /**
     * @brief Get singleton instance
     */
    static CalibrationService *getInstance();
    
    /**
     * @brief Initialize the calibration service
     * 
     * @return true if successful, false otherwise
     */
    bool init();
    
    /**
     * @brief Deinitialize the calibration service
     */
    void deinit();
    
    /**
     * @brief Write calibration data to the ABF1 characteristic using binary packed format
     * 
     * @param data Pointer to calibration data structure
     * @return true if successful, false otherwise
     */
    bool writeCalibrationData(const CalibrationData *data);
    
    /**
     * @brief Read calibration data from the ABF2 characteristic (async)
     * 
     * @param data Pointer to calibration data structure to populate with cached data immediately
     * @param callback Optional callback invoked when actual data is loaded from device
     * @return true if read request sent successfully, false otherwise
     */
    bool readCalibrationData(CalibrationData *data, DataLoadedCallback callback = nullptr);
    
    /**
     * @brief Get the current calibration data (cached)
     * 
     * @return Reference to current calibration data
     */
    const CalibrationData& getCurrentCalibrationData() const { return _current_data; }
    

private:
    CalibrationService();
    ~CalibrationService();
    
    // Singleton instance
    static CalibrationService *_instance;
    
    // Current calibration data (cached)
    CalibrationData _current_data;
    
    // Initialization state
    bool _initialized;
};

} // namespace robo_cat_ears
