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
 * @brief Lighting mode enumeration
 */
enum class LightingMode : uint8_t {
    SOLID = 0,
    BREATHING = 1,
    MARQUEE = 2,
    CHASING = 3,
    RAIN = 4
};

/**
 * @brief RGB color structure (24-bit)
 */
struct RGBColor {
    uint8_t r;
    uint8_t g;
    uint8_t b;
    
    RGBColor() : r(0), g(0), b(0) {}
    RGBColor(uint8_t red, uint8_t green, uint8_t blue) : r(red), g(green), b(blue) {}
    RGBColor(uint32_t color) : r((color >> 16) & 0xFF), g((color >> 8) & 0xFF), b(color & 0xFF) {}
    
    uint32_t toUint32() const { return (r << 16) | (g << 8) | b; }
};

/**
 * @brief Lighting data structure
 */
struct LightingData {
    LightingMode mode;
    uint8_t speed;  // 1-100
    std::vector<RGBColor> colors;  // Max 32 colors
    
    LightingData() : mode(LightingMode::SOLID), speed(50) {}
    
    /**
     * @brief Pack lighting data into binary format for efficient BLE transmission
     * 
     * Format: [mode:1byte][speed:1byte][num_colors:1byte][color1_RGB:3bytes]...[colorN_RGB:3bytes]
     * Max size: 1 + 1 + 1 + (32 * 3) = 99 bytes
     * 
     * @return Packed binary string
     */
    std::string pack() const;
    
    /**
     * @brief Unpack binary data into LightingData structure
     * 
     * @param packed Binary packed string
     * @param data Output LightingData structure
     * @return true if unpacking successful, false otherwise
     */
    static bool unpack(const std::string &packed, LightingData &data);
};

/**
 * @brief Lighting service class
 * 
 * Manages lighting data and communicates with the Bluetooth characteristic ABF1 using binary packed format
 */
class LightingService {
public:
    using DataLoadedCallback = std::function<void(const LightingData &data)>;
    
    bool jsonToLightingData(const std::string &json, LightingData *data);
    /**
     * @brief Get singleton instance
     */
    static LightingService *getInstance();
    
    /**
     * @brief Initialize the lighting service
     * 
     * @return true if successful, false otherwise
     */
    bool init();
    
    /**
     * @brief Deinitialize the lighting service
     */
    void deinit();
    
    /**
     * @brief Write lighting data to the ABF1 characteristic using binary packed format
     * 
     * @param data Pointer to lighting data structure
     * @return true if successful, false otherwise
     */
    bool writeLightingData(const LightingData *data);
    
    /**
     * @brief Read lighting data from the ABF2 characteristic (async)
     * 
     * @param data Pointer to lighting data structure to populate with cached data immediately
     * @param callback Optional callback invoked when actual data is loaded from device
     * @return true if read request sent successfully, false otherwise
     */
    bool readLightingData(LightingData *data, DataLoadedCallback callback = nullptr);
    
    /**
     * @brief Get the current lighting data (cached)
     * 
     * @return Reference to current lighting data
     */
    const LightingData& getCurrentLightingData() const { return _current_data; }
    
    /**
     * @brief Convert mode enum to string
     * 
     * @param mode Lighting mode enum value
     * @return String representation of mode
     */
    static const char* modeToString(LightingMode mode);
    
    /**
     * @brief Convert string to mode enum
     * 
     * @param mode_str String representation of mode
     * @return Lighting mode enum value
     */
    static LightingMode stringToMode(const std::string &mode_str);

private:
    LightingService();
    ~LightingService();
    
    // Singleton instance
    static LightingService *_instance;
    
    // Current lighting data (cached)
    LightingData _current_data;
    
    // Initialization state
    bool _initialized;
};

} // namespace robo_cat_ears
