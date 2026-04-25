#!/usr/bin/env python3
"""
Generate a simple system info icon - cyan/blue gradient with rounded corners
"""

import sys

def generate_system_info_icon(output_file, radius=20):
    """Generate a system info icon with rounded corners."""
    
    width = 126
    height = 126
    
    # Cyan/blue colors for system info
    bg_color_r = 100
    bg_color_g = 180
    bg_color_b = 255
    
    print(f"Generating {width}x{height} system info icon with radius {radius}")
    
    # Generate pixel data
    pixels = []
    transparent_count = 0
    
    for y in range(height):
        for x in range(width):
            # Check which corner region this pixel is in and if it's outside the corner radius
            is_transparent = False
            
            # Top-left corner
            if x < radius and y < radius:
                corner_center_x = radius
                corner_center_y = radius
                distance = ((x - corner_center_x) ** 2 + (y - corner_center_y) ** 2) ** 0.5
                is_transparent = distance > radius
            
            # Top-right corner
            elif x >= (width - radius) and y < radius:
                corner_center_x = width - radius
                corner_center_y = radius
                distance = ((x - corner_center_x) ** 2 + (y - corner_center_y) ** 2) ** 0.5
                is_transparent = distance > radius
            
            # Bottom-left corner
            elif x < radius and y >= (height - radius):
                corner_center_x = radius
                corner_center_y = height - radius
                distance = ((x - corner_center_x) ** 2 + (y - corner_center_y) ** 2) ** 0.5
                is_transparent = distance > radius
            
            # Bottom-right corner
            elif x >= (width - radius) and y >= (height - radius):
                corner_center_x = width - radius
                corner_center_y = height - radius
                distance = ((x - corner_center_x) ** 2 + (y - corner_center_y) ** 2) ** 0.5
                is_transparent = distance > radius
            
            if is_transparent:
                transparent_count += 1
                pixels.append(f"0X{bg_color_r:02X},0X{bg_color_g:02X},0X{bg_color_b:02X},0X00")
            else:
                pixels.append(f"0X{bg_color_r:02X},0X{bg_color_g:02X},0X{bg_color_b:02X},0XFF")
    
    print(f"Generated {len(pixels)} pixels ({transparent_count} transparent)")
    
    # Format output with proper structure
    output_lines = []
    output_lines.append('#ifdef __has_include')
    output_lines.append('#if __has_include("lvgl.h")')
    output_lines.append('#ifndef LV_LVGL_H_INCLUDE_SIMPLE')
    output_lines.append('#define LV_LVGL_H_INCLUDE_SIMPLE')
    output_lines.append('#endif')
    output_lines.append('#endif')
    output_lines.append('#endif')
    output_lines.append('')
    output_lines.append('#if defined(LV_LVGL_H_INCLUDE_SIMPLE)')
    output_lines.append('#include "lvgl.h"')
    output_lines.append('#else')
    output_lines.append('#include "lvgl/lvgl.h"')
    output_lines.append('#endif')
    output_lines.append('')
    output_lines.append('#ifndef LV_ATTRIBUTE_MEM_ALIGN')
    output_lines.append('#define LV_ATTRIBUTE_MEM_ALIGN')
    output_lines.append('#endif')
    output_lines.append('')
    output_lines.append('#ifndef LV_ATTRIBUTE_IMAGE_ESP_BROOKESIA_APP_ICON_LAUNCHER_SYSTEM_INFO_112_112')
    output_lines.append('#define LV_ATTRIBUTE_IMAGE_ESP_BROOKESIA_APP_ICON_LAUNCHER_SYSTEM_INFO_112_112')
    output_lines.append('#endif')
    output_lines.append('')
    output_lines.append('const LV_ATTRIBUTE_MEM_ALIGN LV_ATTRIBUTE_LARGE_CONST LV_ATTRIBUTE_IMAGE_ESP_BROOKESIA_APP_ICON_LAUNCHER_SYSTEM_INFO_112_112 uint8_t esp_brookesia_app_icon_launcher_system_info_112_112_map[] = {')
    
    # Format pixel data in rows of 16 values
    for i in range(0, len(pixels), 4):
        line_pixels = pixels[i:i+4]
        output_lines.append('    ' + ','.join(line_pixels) + ',')
    
    output_lines.append('};')
    output_lines.append('')
    output_lines.append('const lv_image_dsc_t esp_brookesia_app_icon_launcher_system_info_112_112 = {')
    output_lines.append('    .header = {')
    output_lines.append('        .magic = LV_IMAGE_HEADER_MAGIC,')
    output_lines.append('        .cf = LV_COLOR_FORMAT_ARGB8888,')
    output_lines.append('        .flags = 0,')
    output_lines.append(f'        .w = {width},')
    output_lines.append(f'        .h = {height},')
    output_lines.append(f'        .stride = {width * 4},')
    output_lines.append('    },')
    output_lines.append('    .data = esp_brookesia_app_icon_launcher_system_info_112_112_map,')
    output_lines.append(f'    .data_size = {len(pixels) * 4},')
    output_lines.append('};')
    
    # Write to file
    with open(output_file, 'w') as f:
        f.write('\n'.join(output_lines))
    
    print(f"✓ Saved to {output_file}")

if __name__ == '__main__':
    if len(sys.argv) < 2:
        print("Usage: python3 generate_system_info_icon.py <output.c> [radius]")
        sys.exit(1)
    
    radius = 20
    if len(sys.argv) >= 3:
        radius = int(sys.argv[2])
    
    generate_system_info_icon(sys.argv[1], radius)
