/*
#   Copyright (C) 2024 Sean D'Epagnier
#
# This Program is free software; you can redistribute it and/or
# modify it under the terms of the GNU General Public
# License as published by the Free Software Foundation; either
# version 3 of the License, or (at your option) any later version.  
*/

function request() {
    var data_type = document.getElementById('data_type');
    var data_range = document.getElementById('data_range');
    
    const url = 'history?data_type=' + data_type.value + '&data_range=' + data_type.range;

    fetch(url)
        .then(response => {
            if (!response.ok) {
                // If response is not OK, throw an error to skip to the catch block
                throw new Error('Network response was not ok ' + response.statusText);
            }
            // If response is OK, parse and return JSON data
            return response.json();
        })
        .then(data => {
            // Handle the JSON data here
            plot(data);
        })
        .catch(error => {
            // Handle any errors that occurred during the fetch or parsing
            console.error('There was a problem with the fetch operation:', error);
        });
}

function plot(data)
{
    let tt = data['total_time'];

    document.getElementById('time').innerText = tt;
    document.getElementById('high').innerText = data['high'];
    document.getElementById('low').innerText = data['low'];

    let d = data['data'];
    let xValues = [];
    let data1 = [];
    for(var i=0; i<=10; i++)
	xValues.push(tt/i);

    for(var i=0; i<d.length(); i++)
        data1.push({x:d['time'], y:d['value']});

    if(theChart)
        theChart.destroy();
    theChart = new Chart("chart", {
        type: "line",
        data: {
//            labels: xValues,
            datasets: [{
                data: data1,
                borderColor: "red",
                fill: false,
                pointRadius: 0
            }]
        },
        options: {
            legend: {display: false},
            scales: {
                x: {
                    title: {
                            display: true,
                            text: "Time"
                        }
                    },
                    y: {
                        title: {
                            display: true,
                            text: "value"
                        }
                    }
                }
            }
            
    });
}
