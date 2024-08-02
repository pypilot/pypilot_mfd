#!/usr/bin/env python3
#
#   Copyright (C) 2024 Sean D'Epagnier
#
# This Program is free software; you can redistribute it and/or
# modify it under the terms of the GNU General Public
# License as published by the Free Software Foundation; either
# version 3 of the License, or (at your option) any later version.  

import sys, os

for filename in sys.argv[1:]:
    f = open(filename, 'rb')
    data = bytearray(f.read())
    f.close();

    varname = os.path.basename(filename).replace('.', '_')

    print('const char ' + varname + '[] PROGMEM = {');
    for byte in data:
        sys.stdout.write('0x%x, ' % byte);
    print('};')
    print('')
    sys.stderr.write('wrote file ' + filename + '\t\tsize: ' + str(len(data)) + ' bytes\n')
