/*
#   Copyright (C) 2024 Sean D'Epagnier
#
# This Program is free software; you can redistribute it and/or
# modify it under the terms of the GNU General Public
# License as published by the Free Software Foundation; either
# version 3 of the License, or (at your option) any later version.  
*/

var canvas = document.getElementById('canvas');
const ctx = canvas.getContext('2d');

var gateway = `ws://${window.location.hostname}/ws`;
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

function handleOffsetChange(e) {
    let mac = e.getAttribute('data-mac');
    msg = {mac: {'offset': e.value}};
    websocket.send(JSON.stringify(msg));
}

function handlePositionChange(e) {
    let mac = e.getAttribute('data-mac');
    msg = {mac: {'position': e.value}};
    websocket.send(JSON.stringify(msg));
}

function circle(x, y, r) {
    ctx.ellipse(x, y, r, r, 0, 0, Math.PI*2);
}

function rad(x) {
    return x/180*Math.PI;
}

function render(direction, knots)
{
    let w = canvas.width;
    let h = canvas.height;
    let u = w/20;
    let r = w/2-u;

    ctx.reset();
    ctx.lineWidth=5;
    circle(w/2, h/2, r);
    ctx.stroke();

    if(direction != null) {
        let x = Math.sin(rad(direction));
        let y = Math.cos(rad(direction));

        ctx.beginPath();
        ctx.moveTo(w/2-u*y, h/2-u*x);
        ctx.lineTo(w/2+x*r, h/2-y*r);
        ctx.lineTo(w/2+u*y, h/2+u*x);
        ctx.fill(); // Render the arrow
    }

    ctx.font = Math.round(w/4) + 'px serif';
    ctx.fillText(Math.round(knots) +' kt', w*.4, h*.7);    
}

var wind_sensors = {};
// Function that receives the message from the ESP32 with the readings
function onMessage(event) {
    //console.log(event.data);
    var data = event.data;
    if(data == 'reload')
        location.reload(); // refresh page
    else {
        var msg = JSON.parse(data);
        if('wind' in msg)
            onWindMessage(msg['wind']);
    }
}

function onWindMessage(msg) {
    // message just updates active display
    if('direction' in msg) {
        let dirt = msg['direction'] === null ? '---' : msg['direction'].toFixed(1);
        document.getElementById('wind_direction').innerText = dirt;
        document.getElementById('wind_knots').innerText = msg['knots'].toFixed(2);
        render(msg['direction'], msg['knots']);
        return;
    }
    
    function inp(mac, v) {
        var options = '';
        for(o of ['Primary', 'Secondary', 'Port', 'Starboard', 'Ignored'])
            options += '<option value="' + o + '"' + (v == o ? ' selected' : '') + '>' + o + '</options>';
        return '<select data-mac="' + mac + '"style="width: 100%;" oninput="handlePositionChange(this)">' + options + '</select>';
    }

    function dir(mac, v) {
        return '<input type="number" data-mac="' + mac +
            '" min="0" max="360" step="2" value="0" onchange="handleOffsetChange(this)">';
    }
    
    var table = document.getElementById('wind_sensors_table');
    //mac Position Offset Direction Speed Latency
    for (var mac in msg) {
        let v = msg[mac];
        if(!(mac in wind_sensors)) {
            let row = table.insertRow(1);
            row.insertCell(-1).innerText = mac;
            row.insertCell(-1).innerHTML = inp(mac, v['position']);
            row.insertCell(-1).innerHTML = dir(mac, v['offset']);
            row.insertCell(-1);
            row.insertCell(-1);
            row.insertCell(-1);
            wind_sensors[mac] = row;
        }
        let row = wind_sensors[mac];
        row.cells[3].innerText = (v['dir'] === null) ? '---' : v['dir'].toFixed(1) + ' deg';
        row.cells[4].innerText = v['knots'].toFixed(2) + 'kt';
        row.cells[5].innerText = v['dt'] + 'ms';
    }

    // remove any sensors we no longer receive
    for(mac in wind_sensors)
        if(!(mac in msg)) {
            table.deleteRow(wind_sensors[mac].rowIndex);
            delete wind_sensors[mac];
        }
}
