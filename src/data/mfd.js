/*  Copyright (C) 2026 Sean D'Epagnier
#
# This Program is free software; you can redistribute it and/or
# modify it under the terms of the GNU General Public
# License as published by the Free Software Foundation; either
# version 3 of the License, or (at your option) any later version.  
*/

function on_display_settings() {
    post(['use_360', 'use_fahrenheit', 'use_inHg', 'use_depthft',
          'lat_lon_format', 'invert', 'contrast', 'backlight', 'mirror', 'power_button']);
}

function on_display_pages() {
    let div = gei("display_pages");

    let ids = [];
    div.querySelectorAll("[id]").forEach(el => {
        ids.push(el.id);
    });
    post(ids);
}

function on_alarm_settings() {
    post(["anchor_alarm", "anchor_alarm_distance", "course_alarm", "course_alarm_course", "course_alarm_error", "gps_speed_alarm", "gps_min_speed_alarm_knots", "gps_max_speed_alarm_knots", "wind_speed_alarm", "wind_min_speed_alarm_knots", "wind_max_speed_alarm_knots", "water_speed_alarm", "water_min_speed_alarm_knots", "water_max_speed_alarm_knots", "weather_alarm_pressure", "weather_alarm_min_pressure", "weather_alarm_pressure_rate", "weather_alarm_pressure_rate_value", "weather_alarm_lightning", "weather_alarm_lightning_distance", "depth_alarm", "depth_alarm_min", "depth_alarm_rate", "depth_alarm_rate_value", "ais_alarm", "ais_alarm_cpa", "ais_alarm_tcpa", "pypilot_alarm_noconnection", "pypilot_alarm_fault", "pypilot_alarm_no_imu", "pypilot_alarm_no_motor_controller", "pypilot_alarm_lost_mode"]);
}

fetch('/display_pages')
    .then(r => r.text())
    .then(html => {
        document.getElementById('display_pages').innerHTML = html;
    });
