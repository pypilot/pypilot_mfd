/*
#   Copyright (C) 2024 Sean D'Epagnier
#
# This Program is free software; you can redistribute it and/or
# modify it under the terms of the GNU General Public
# License as published by the Free Software Foundation; either
# version 3 of the License, or (at your option) any later version.  
*/

var gateway = `ws://${window.location.hostname}/ws_data`;
var websocket;
// Init web socket when the page loads
window.addEventListener('load', onload);

function onload(event) {
    initWebSocket();
}

var init_timeout = 1;
function initWebSocket() {
    console.log('Trying to open a WebSocket connection…');
    websocket = new WebSocket(gateway);
    websocket.onopen = onOpen;
    websocket.onclose = onClose;
    websocket.onmessage = onMessage;
}

// When websocket is established, call the getReadings() function
function onOpen(event) {
    console.log('Connection opened');
    init_timeout = 1;
}

function onClose(event) {
    console.log('Connection closed');
    setTimeout(initWebSocket, init_timeout*1000);
    init_timeout*=2;
    if(init_timeout > 10)
        init_timeout = 10;
}

var data_displays = {};
// Function that receives the message from the ESP32 with the readings
function onMessage(event) {
    //console.log(event.data);
    var msg = JSON.parse(event.data);

    var table = document.getElementById('display_data_table');
    //mac Position Offset Direction Speed Latency
    for (var name in msg) {
        let v = msg[name];
        if(!(name in data_displays)) {
            let row = table.insertRow(1);
            row.insertCell(-1).innerText = name;
            row.insertCell(-1); // value
            row.insertCell(-1); // source
            row.insertCell(-1); // latency
            data_displays[name] = row;
        }
        let row = data_displays[name];
        row.cells[1].innerText = Math.round(1000*v['value'])/1000;
        row.cells[2].innerText = v['source'];
        row.cells[3].innerText = v['latency'] + 'ms';
    }

    // remove any sensors we no longer receive
    for(name in data_displays)
        if(!(name in msg)) {
            table.deleteRow(data_displays[name].rowIndex);
            delete data_displays[name];
        }
}
