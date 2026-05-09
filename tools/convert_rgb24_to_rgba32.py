#!/usr/bin/env python3
"""
Convert 24-bit RGB image array to 32-bit RGBA by adding alpha channel.
"""

import argparse
import re
import sys

def convert_rgb24_to_rgba32(input_file, output_file, corner_radius=20):
    """Convert RGB (3 bytes) to RGBA (4 bytes) by adding 0xFF alpha."""
    with open(input_file, 'r') as f:
        content = f.read()
    
    # Find the array declaration
    array_pattern = r'(const\s+unsigned\s+char\s+[\w\-]+)\[(\d+)\]'
    match = re.search(array_pattern, content)
    
    if not match:
        print("Error: Could not find array declaration")
        sys.exit(1)
    
    old_name = match.group(1)
    old_size = int(match.group(2))
    new_size = (old_size // 3) * 4  # Convert from 3 bytes/pixel to 4 bytes/pixel
    
    # Extract array data
    data_start = content.find('{')
    data_end = content.find('};')
    
    before = content[:data_start+1]
    data_section = content[data_start+1:data_end]
    
    # Remove comment if present (e.g., /* 0X10,0X18,... */)
    data_section = re.sub(r'/\*[^*]*\*/', '', data_section)
    after = '};'
    
    # Extract all hex bytes
    hex_pattern = r'0X[0-9A-F]{2}'
    hex_values = re.findall(hex_pattern, data_section)
    
    print(f"Found {len(hex_values)} bytes ({len(hex_values)//3} RGB pixels)")
    
    # Calculate image dimensions for rounded corners
    image_width = 126  # Known width for this icon
    image_height = len(hex_values) // (image_width * 3)
    
    # Corner centers are at (r, r) from each corner
    # Top-left corner center: (corner_radius, corner_radius)
    # Top-right corner center: (width - corner_radius, corner_radius)
    # Bottom-left corner center: (corner_radius, height - corner_radius)
    # Bottom-right corner center: (width - corner_radius, height - corner_radius)
    
    print(f"Image size: {image_width}x{image_height}, Corner radius: {corner_radius}")
    
    # Convert BGR to RGBA by swapping R and B channels, adding alpha channel
    # Make corner pixels outside radius transparent for rounded corners
    rgba_values = []
    pixel_index = 0
    transparent_count = 0
    
    for i in range(0, len(hex_values), 3):
        if i + 2 < len(hex_values):
            b_val = hex_values[i]      # B in source
            g_val = hex_values[i+1]    # G in source
            r_val = hex_values[i+2]    # R in source
            
            # Calculate x, y position
            x = pixel_index % image_width
            y = pixel_index // image_width
            
            # Check which corner region this pixel is in and if it's outside the corner radius
            is_transparent = False
            
            # Top-left corner
            if x < corner_radius and y < corner_radius:
                corner_center_x = corner_radius
                corner_center_y = corner_radius
                distance = ((x - corner_center_x) ** 2 + (y - corner_center_y) ** 2) ** 0.5
                is_transparent = distance > corner_radius
            
            # Top-right corner
            elif x >= (image_width - corner_radius) and y < corner_radius:
                corner_center_x = image_width - corner_radius
                corner_center_y = corner_radius
                distance = ((x - corner_center_x) ** 2 + (y - corner_center_y) ** 2) ** 0.5
                is_transparent = distance > corner_radius
            
            # Bottom-left corner
            elif x < corner_radius and y >= (image_height - corner_radius):
                corner_center_x = corner_radius
                corner_center_y = image_height - corner_radius
                distance = ((x - corner_center_x) ** 2 + (y - corner_center_y) ** 2) ** 0.5
                is_transparent = distance > corner_radius
            
            # Bottom-right corner
            elif x >= (image_width - corner_radius) and y >= (image_height - corner_radius):
                corner_center_x = image_width - corner_radius
                corner_center_y = image_height - corner_radius
                distance = ((x - corner_center_x) ** 2 + (y - corner_center_y) ** 2) ** 0.5
                is_transparent = distance > corner_radius
            
            if is_transparent:
                transparent_count += 1
            
            rgba_values.append(r_val)              # R
            rgba_values.append(g_val)              # G
            rgba_values.append(b_val)              # B
            rgba_values.append('0X00' if is_transparent else '0XFF')  # A
            
            pixel_index += 1
    
    print(f"Made {transparent_count} pixels transparent in corners (radius {corner_radius})")
    
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
    output_lines.append('')
    output_lines.append('#ifndef LV_ATTRIBUTE_MEM_ALIGN')
    output_lines.append('#define LV_ATTRIBUTE_MEM_ALIGN')
    output_lines.append('#endif')
    output_lines.append('')
    output_lines.append('#ifndef LV_ATTRIBUTE_IMAGE_ESP_BROOKESIA_APP_ICON_LAUNCHER_ROBO_CAT_EARS_112_112')
    output_lines.append('#define LV_ATTRIBUTE_IMAGE_ESP_BROOKESIA_APP_ICON_LAUNCHER_ROBO_CAT_EARS_112_112')
    output_lines.append('#endif')
    output_lines.append('')
    
    # Array declaration
    output_lines.append('const LV_ATTRIBUTE_MEM_ALIGN LV_ATTRIBUTE_LARGE_CONST LV_ATTRIBUTE_IMAGE_ESP_BROOKESIA_APP_ICON_LAUNCHER_ROBO_CAT_EARS_112_112 uint8_t esp_brookesia_app_icon_launcher_robo_cat_ears_112_112_map[] = {')
    
    # Format data in lines of 16 bytes (4 pixels)
    for i in range(0, len(rgba_values), 16):
        line_values = rgba_values[i:i+16]
        line = '    ' + ','.join(line_values) + ','
        output_lines.append(line)
    
    output_lines.append('};')
    output_lines.append('')
    output_lines.append('')
    output_lines.append('')
    output_lines.append('const lv_image_dsc_t esp_brookesia_app_icon_launcher_robo_cat_ears_112_112 = {')
    output_lines.append('    .header.cf = LV_COLOR_FORMAT_ARGB8888,')
    output_lines.append('    .header.magic = LV_IMAGE_HEADER_MAGIC,')
    output_lines.append('    .header.w = 126,')
    output_lines.append('    .header.h = 126,')
    output_lines.append(f'    .data_size = {len(rgba_values)},')
    output_lines.append('    .data = esp_brookesia_app_icon_launcher_robo_cat_ears_112_112_map,')
    output_lines.append('};')
    
    # Write output
    with open(output_file, 'w') as f:
        f.write('\n'.join(output_lines))
    
    print(f"✓ Converted to {len(rgba_values)} bytes ({len(rgba_values)//4} RGBA pixels)")
    print(f"✓ Saved to {output_file}")

if __name__ == '__main__':
    parser = argparse.ArgumentParser(description='Convert RGB24 to RGBA32 with rounded corners')
    parser.add_argument('input_file', help='Input C file with RGB24 array')
    parser.add_argument('output_file', help='Output C file with RGBA32 array')
    parser.add_argument('-r', '--radius', type=float, default=20.0, 
                        help='Corner radius in pixels (default: 20.0)')
    
    args = parser.parse_args()
    convert_rgb24_to_rgba32(args.input_file, args.output_file, args.radius)
