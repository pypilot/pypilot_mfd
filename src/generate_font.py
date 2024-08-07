#!/usr/bin/env python
#
#   Copyright (C) 2024 Sean D'Epagnier
#
# This Program is free software; you can redistribute it and/or
# modify it under the terms of the GNU General Public
# License as published by the Free Software Foundation; either
# version 3 of the License, or (at your option) any later version.

import time, sys

try:
    from PIL import Image
    from PIL import ImageDraw
    from PIL import ImageFont
    from PIL import ImageChops
    
except Exception as e:
    print('failed to load PIL to create fonts, aborting...', e)
    exit(0)

fontpath='font.ttf'

def varname(size, c):
    return 'character_' + str(size) + '_' + str(ord(c));

total_bytes=0
max_count=0
def create_character(sz, c):
    ifont = ImageFont.truetype(fontpath, sz)
    try:
        size = ifont.getsize(c)
    except:  ## support newer pillow
        left, top, right, bottom = ifont.getbbox(c)
        size = right, bottom        
    image = Image.new('RGBA', size)
    draw = ImageDraw.Draw(image)
    draw.text((0, 0), c, font=ifont)

    '''
    if crop:
        bg = Image.new(image.mode, image.size, image.getpixel((0, 0)))
        diff = ImageChops.difference(image, bg)
        bbox = diff.getbbox()
        if bbox:
            image = image.crop(bbox)
    '''

    sys.stdout.write('static const uint8_t ' + varname(sz, c) + '_data[] PROGMEM = {')
    cnt = 20
    data = list(image.getdata())
    last = int(data[0][3]/32) # 3bpp
    count = 1
    cnt = 20
    byte = 0;
    def write_bytes():
        global total_bytes, max_count
        nonlocal count, cnt, byte
        if count > max_count:
            max_count = count
        color = last
        if count <= 16:
            sys.stdout.write(('0x%x'% (color | ((count-1)<<3)) + ', '))
            total_bytes+=1
            cnt+=1
            byte+=1
        else:
            while count:
                ourcnt = min(count, 4096)
                sys.stdout.write(('0x%x'% (color | ((ourcnt%16)<<3) | 0x80)) + ', ')
                sys.stdout.write(('0x%x'% int(ourcnt/16-1) + ', '))                
                count -= ourcnt
                cnt+=2
                total_bytes+=2
                byte+=2
        if cnt >= 10:
            sys.stdout.write('\n    ')
            cnt = 0
                
    
    for i in range(1, len(data)):
        v = int(data[i][3]/32)
        if v == last:
            count+=1
        else:
            write_bytes()
            count = 1
            last = v
    write_bytes()
    print('};\n')

    return size[0], size[1], top, byte


characters = 'abcdefghijklmnopqrstuvwxyz1234567890`~!@#$%^&*()-=_+,./<>?[]{}|;\':" ABCDEFGHIJKLMNOPQRSTUVWXYZ'
big_characters = '1234567890.~- '
#big_characters = characters

sizes = [21, 24, 30, 36, 42, 52, 66, 82, 108, 134, 162, 200, 260, 340]

ords = list(map(ord, characters))

minord = min(ords)
maxord = max(ords)

print('#define FONT_MIN ' + str(minord))
print('#define FONT_MAX ' + str(maxord))
print('#define FONT_COUNT ' + str(len(sizes)))

def main():
    print('/* This file is generated by generate_font.py */')
    
    print('struct character {')
    print('   int w, h, yoff, size;')
    print('   const uint8_t *data;')
    print('};\n')

    print('struct font {')
    print('   int h;')
    print('   const character *font_data;')
    print('};\n')

    for sz in sizes:
        data = {}
        font_bytes = 0
        this_characters = characters if sz < 180 else big_characters
        for c in this_characters:
            data[ord(c)] = create_character(sz, c);

        print('\n')
        print('static const character font_' + str(sz) + '[] PROGMEM = {')
        for i in range(minord, maxord+1):
            if i in data:
                d = data[i]
                font_bytes += d[3]
                print('    { ' + str(d[0]) + ', ' + str(d[1]) + ', ' + str(d[2]) + ', ' + str(d[3]) + ', ' + varname(sz, '%c' % i) + '_data },')
            else:
                print('    { 0, 0, 0, 0, 0 },')
        print('};\n')
        sys.stderr.write('font ' + str(sz) + ' ' + str(font_bytes) + '\n')

    print('static const font fonts[FONT_COUNT] PROGMEM = {')
    for sz in sizes:
        print('    {' + str(sz) + ', font_' + str(sz) + '},')
    print('};\n')
        
    print('// total bytes ' + str(total_bytes))
    print('// max count ' + str(max_count))

if __name__ == '__main__':
    main()
