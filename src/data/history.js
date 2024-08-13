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
    const url = 'history?data_type=' + data_type.value + '&data_range=' + data_range.value;
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

var theChart = null;
function plot(data)
{
    let tt = data['total_time'];

    document.getElementById('time').innerText = tt;
    document.getElementById('high').innerText = data['high'];
    document.getElementById('low').innerText = data['low'];

    let d = data['data'];

    let datasets = [];
    let data1 = [];
    let mint = -1, maxt = -1;

    function push_data() {
        data1.reverse();
        if(data1.length)
            datasets.push({
                data: data1,
                borderColor: "red",
                fill: false,
                pointRadius: 0,
                showLine: true
            });
        data1 = [];
    }    

    for(var i=0; i<d.length; i++) {
        let e = d[i];
        if(typeof e[1] === "boolean") {
            push_data();
        } else {
            let t = e[0];
            let v = e[1];
            data1.push({x:t, y:v});
            if(maxt == -1 || t > maxt)
                maxt = t;
            if(mint == -1 || t < mint)
                mint = t;
        //    console.log('t: ' + t + ' v: ' + v);
        }
    }
    push_data();
        
    
    let xValues = [];
    for(var i=0; i<=10; i++)
	xValues.push(mint+i*(maxt-mint)/10);


    if(theChart) {
        theChart.data.datasets = datasets;
        theChart.update();
    } else {
        theChart = new Chart("chart", {
            type: "scatter",
            data: {
                labels: xValues,
                datasets: datasets
            },
            options: {
                responsive: true,
                maintainAspectRatio: false,
                animation: {
                    duration: 0
                },
                legend: {display: false},
                plugins: {
                    legend: {display: false},
                    title: {
                        display: false,
                        text: 'Custom Chart Title'
                    }
                },
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
                            text: "Value"
                        }
                    }
                }
            }
        });
    }

    setTimeout(request, 1000*tt/80);
}

// initial request
request();
