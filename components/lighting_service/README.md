# Lighting Service Component

## Overview

The lighting service component manages lighting data for the Robo Cat Ears controller. It provides JSON serialization/deserialization for lighting configurations and communicates with the **ABF2** Bluetooth characteristic for lighting control, while **ABF1** remains dedicated to animation control.

## Features

- **Lighting Modes**: Solid, Breathing, Marquee, Chasing, Rain
- **Speed Control**: 1-100 speed range for animation control
- **Color Management**: Up to 32 RGB colors (24-bit)
- **JSON Serialization**: Automatic conversion between struct and JSON format
- **Bluetooth Integration**: Uses bluetooth_service to read/write ABF2 characteristic
- **Separate Control**: ABF1 (animation) and ABF2 (lighting) characteristics for independent control

## Usage

### Initialize the Service

```cpp
#include "lighting_service.hpp"

robo_cat_ears::LightingService *service = robo_cat_ears::LightingService::getInstance();
service->init();
```

### Create Lighting Data

```cpp
robo_cat_ears::LightingData data;
data.mode = robo_cat_ears::LightingMode::BREATHING;
data.speed = 75;
data.colors.push_back(robo_cat_ears::RGBColor(255, 0, 0));    // Red
data.colors.push_back(robo_cat_ears::RGBColor(0, 255, 0));    // Green
data.colors.push_back(robo_cat_ears::RGBColor(0, 0, 255));    // Blue
```

### Write to Device

```cpp
service->writeLightingData(&data);
```

### Read from Device

```cpp
robo_cat_ears::LightingData data;
service->readLightingData(&data);
```

### JSON Format

```json
{
  "mode": "Breathing",
  "speed": 75,
  "colors": [
    {"r": 255, "g": 0, "b": 0},
    {"r": 0, "g": 255, "b": 0},
    {"r": 0, "g": 0, "b": 255}
  ]
}
```

## Dependencies

- `bluetooth_service` - For BLE communication
- `json` (cJSON) - For JSON parsing and serialization

## API Reference

### LightingMode Enum

- `SOLID` - Static color display
- `BREATHING` - Pulsing animation
- `MARQUEE` - Scrolling pattern
- `CHASING` - Sequential animation
- `RAIN` - Raindrop effect

### LightingData Struct

- `mode` - Current lighting mode
- `speed` - Animation speed (1-100)
- `colors` - Vector of RGB colors (max 32)

### Main Methods

- `init()` - Initialize the service
- `deinit()` - Clean up resources
- `writeLightingData(data)` - Send lighting data to device
- `readLightingData(data)` - Receive lighting data from device
