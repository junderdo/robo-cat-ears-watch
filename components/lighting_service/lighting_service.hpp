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
};

/**
 * @brief Lighting service class
 * 
 * Manages lighting data and communicates with the Bluetooth characteristic ABF2
 */
class LightingService {
public:
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
     * @brief Write lighting data to the ABF2 characteristic
     * 
     * @param data Pointer to lighting data structure
     * @return true if successful, false otherwise
     */
    bool writeLightingData(const LightingData *data);
    
    /**
     * @brief Read lighting data from the ABF2 characteristic
     * 
     * @param data Pointer to lighting data structure to populate
     * @return true if successful, false otherwise
     */
    bool readLightingData(LightingData *data);
    
    /**
     * @brief Convert lighting data to JSON string
     * 
     * @param data Pointer to lighting data structure
     * @return JSON string representation
     */
    std::string lightingDataToJson(const LightingData *data);
    
    /**
     * @brief Parse JSON string to lighting data
     * 
     * @param json JSON string to parse
     * @param data Pointer to lighting data structure to populate
     * @return true if successful, false otherwise
     */
    bool jsonToLightingData(const std::string &json, LightingData *data);
    
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
