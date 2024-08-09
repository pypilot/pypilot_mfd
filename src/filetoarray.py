#!/usr/bin/env python3
#
#   Copyright (C) 2024 Sean D'Epagnier
#
# This Program is free software; you can redistribute it and/or
# modify it under the terms of the GNU General Public
# License as published by the Free Software Foundation; either
# version 3 of the License, or (at your option) any later version.  

import sys, os

file_list = {};
for filename in sys.argv[1:]:
    f = open(filename, 'rb')
    data = bytearray(f.read())
    f.close();

    fname = os.path.basename(filename)
    varname = fname.replace('.', '_')

    print('const char ' + varname + '[] PROGMEM = {');
    for byte in data:
        sys.stdout.write('0x%x, ' % byte);
    print('};')
    print('')
    sys.stderr.write('wrote file ' + fname + '\t\tsize: ' + str(len(data)) + ' bytes\n')
    file_list[varname] = {'path': fname, 'size': str(len(data))};

print('')
print('struct data_file {')
print('    const char *path;')
print('    int size;')
print('    const char *data;')
print('};')
print('')
print('const data_file data_files[] = {')

for varname, data in file_list.items():
    print('{"' + data['path'] + '", ' + data['size'] + ', ' + varname + '},')

print('};')

