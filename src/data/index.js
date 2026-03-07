/*  Copyright (C) 2026 Sean D'Epagnier
#
# This Program is free software; you can redistribute it and/or
# modify it under the terms of the GNU General Public
# License as published by the Free Software Foundation; either
# version 3 of the License, or (at your option) any later version.  
*/

fetch('/display_pages')
    .then(r => r.text())
    .then(html => {
        document.getElementById('display_pages').innerHTML = html;
    });
