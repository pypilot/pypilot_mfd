/*  Copyright (C) 2026 Sean D'Epagnier
#
# This Program is free software; you can redistribute it and/or
# modify it under the terms of the GNU General Public
# License as published by the Free Software Foundation; either
# version 3 of the License, or (at your option) any later version.  
*/

function gei(id) {
    return document.getElementById(id);
}

var canvas = gei('canvas');
const ctx = canvas.getContext('2d');

var websocket;
// Init web socket when the page loads
window.addEventListener('load', onload);

function onload(event) {
    initWebSocket();
}

var init_timeout = 1;
function initWebSocket() {
    console.log('Trying to open a WebSocket connection');
    var gateway = `ws://${window.location.hostname}/ws`;
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
    init_timeout++;
    if(init_timeout > 3)
        init_timeout = 3;
}

function handleOffsetChange(e) {
    let mac = e.getAttribute('data-mac');
    msg = {transmitters: {wind: {[mac]: {offset: Number(e.value)}}}};
    websocket.send(JSON.stringify(msg));
}

function handlePositionChange(e) {
    let mac = e.getAttribute('data-mac');
    msg = {transmitters: {wind: {[mac]: {position: e.value}}}};
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
function toBool(v) {
    if (typeof v === "boolean") return v;
    if (typeof v === "number") return v !== 0;
    if (typeof v === "string") {
        const s = v.toLowerCase();
        return s === "1" || s === "true" || s === "yes" || s === "on";
    }
    return false;
}

function setElementValue(name, value) {
    let el = gei(name);
    if(!el)
        return;
    
    const tag = el.tagName.toLowerCase();

    if (tag === "input") {
        const type = (el.type || "").toLowerCase();

        if (type === "checkbox")
            el.checked = toBool(value);
        else
            el.value = value ?? "";
    } else if (tag === "textarea" || tag === "select")
        el.value = value ?? "";
    else
        el.textContent = value ?? "";
}

function onMessage(event) {
    var data = event.data;

    var msg = JSON.parse(data);
    if('wind' in msg)
        onWindMessage(msg['wind']);

    if('wifi_networks' in msg) {
        let table = gei('wifi_networks_table');
        let list = gei('ssids_list');
        if(table === null || list === null)
            return;
        networks = msg['wifi_networks']
        table.innerHTML = '<th>ssid</th><th>channel<th><th>rssi</th><th>encryption</th>';
        list.innerHTML = '';
        for (const n of networks) {
            const row = table.insertRow();
            row.innerHTML = `
        <td>${n.ssid}</td>
        <td>${n.channel}</td>
        <td>${n.rssi}</td>
        <td>${n.encryption}</td>
    `;
            list.innerHTML += `<option value="${n.ssid}">`
        }        
    }

    if('transmitters' in msg)
        onTransmitters(msg['transmitters']);
    
    for (const [key, value] of Object.entries(msg))
        setElementValue(key, value);

    if('wifi_mode' in msg)
        on_wifi_mode();
}

function onWindMessage(msg) {
    // message just updates active display
    if('direction' in msg) {
        let dirt = msg['direction'] === null ? '---' : msg['direction'].toFixed(1);
        gei('wind_direction').innerText = dirt;
    }
    if('knots' in msg)
        gei('wind_knots').innerText = msg['knots'].toFixed(2);

    if('accel' in msg)
        gei('wind_accel').innerText = `x=${msg['accel']['x']} y=${msg['accel']['y']} z=${msg['accel']['z']}`;
    
    if('direction' in msg && 'knots' in msg)
        render(msg['direction'], msg['knots']);
}

function onTransmittersWind(msg) {
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

    var table = gei('wind_sensors_table');
    //mac Position Offset Direction Speed Latency
    for (var mac in msg) {
        let v = msg[mac];
        if(!(mac in wind_sensors)) {
            let row = table.insertRow(1);
            row.innerHTML = '<td></td>'.repeat(10);
            row.cells[0].innerText = mac;
            row.cells[1].innerHTML = inp(mac, v['position']);
            row.cells[2].innerHTML = dir(mac, v['offset']);
            row.cells[9].innerHTML = "<input class='button' type='button' style='width: 100%' value='ap' onclick='on_ap_sensor(\"" + mac + "\")'>";
            wind_sensors[mac] = row;
        }
        let row = wind_sensors[mac];

        if('position' in v)
            row.cells[1].querySelector('select').value = v['position']
        if('offset' in v)
            row.cells[2].querySelector('input').value = v['offset']
        if('direction' in v)
            row.cells[3].innerText = (v['direction'] === null) ? '---' : v['direction'].toFixed(1);
        if('knots' in v)
            row.cells[4].innerText = v['knots'].toFixed(2) + 'kt';
        if('dt' in v)
            row.cells[5].innerText = v['dt'] + 'ms';
        if('rate' in v)
            row.cells[6].innerText = v['rate'] + 'hz';
        if('packet_loss' in v)
            row.cells[7].innerText = v['packet_loss'] + '%';
        if('runtime' in v)
            row.cells[8].innerText = v['runtime'] + 's';
    }

    // remove any sensors we no longer receive
    for(mac in wind_sensors)
        if(!(mac in msg)) {
            table.deleteRow(wind_sensors[mac].rowIndex);
            delete wind_sensors[mac];
        }
}

function onTransmitters(msg) {
    if('wind' in msg)
        onTransmittersWind(msg['wind']);
}

function on_wifi_mode() {
    var mode = gei('wifi_mode').value;
    gei('wifi_ap').style.display = (mode == 'ap') ? 'grid' : 'none';
    gei('wifi_client').style.display = (mode == 'client') ? 'grid' : 'none';
}

function fields(ids) {
    return Object.fromEntries(
        ids.map(id => {
            const el = gei(id);

            let value;
            if (el.type === "checkbox") {
                value = el.checked;   // true / false
            } else {
                value = el.value;
            }

            return [id, value];
        })
    );
}

function post(keys) {
    msg = fields(keys);
    websocket.send(JSON.stringify(msg));
}

function on_wifi_settings() {
    post(['wifi_mode', 'ap_ssid', 'ap_psk', 'client_ssid', 'client_psk', 'wifi_channel']);
}

function on_display_data() {
    post(['output_usb', 'usb_baud_rate',
          'input_nmea_pypilot', 'output_nmea_pypilot',
          //'input_nmea_signalk', 'output_nmea_signalk',
          'input_nmea_tcp_client', 'output_nmea_tcp_client', 'nmea_tcp_client_addr',
          'nmea_tcp_client_port',
          'input_nmea_tcp_server', 'output_nmea_tcp_server',
          'nmea_tcp_server_port',
          //'input_signalk', 'output_signalk',
          'forward_nmea_serial_to_wifi',
          'compensate_wind_with_accelerometer',
          ]);

    //'input_usb'
    //'compute_true_wind_from_gps',          'compute_true_wind_from_water_speed'
    //'forward_nmea_serial_to_serial',
    //'rs422_1_baud_rate', 'rs422_2_baud_rate',
}

function cmd(cmd) {
    msg = {command : cmd};
    websocket.send(JSON.stringify(msg));
}

function on_ap_sensor(mac) {
    cmd("transmitters " + mac + " ap")
    alert('Look for the access point and connect directly to the sensor.');
}
