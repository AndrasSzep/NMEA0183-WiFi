// Complete project details: https://randomnerdtutorials.com/esp32-web-server-websocket-sliders/

var gateway = `ws://${window.location.hostname}/ws`;
var websocket;
window.addEventListener('load', onload);


// Get references to the canvas elements
const tempcanvas = document.querySelector('#tempchart');
const humcanvas = document.querySelector('#humchart');
const prescanvas = document.querySelector('#preschart');
const watercanvas = document.querySelector('#waterchart');

// Create the chart contexts
const tempCtx = tempcanvas.getContext('2d');
const humCtx = humcanvas.getContext('2d');
const presCtx = prescanvas.getContext('2d');
const waterCtx = watercanvas.getContext('2d');

var currentHour = 10; // Replace with the actual current hour value
var lastHour = 8; // Replace with the actual last hour value
var currentMinute = 10; // Replace with the actual current hour value
var lastMinute = 8; // Replace with the actual last hour value

// Generate a random value between 15 and 25
function getRandomValue(a,b) {
    return Math.random() * a + b;
}

const waterchart = new Chart(waterCtx, {
    type: 'line',
    data: {
        labels: Array.from({ length: 24 }, (_, i) => (i-24).toString()),
        datasets: [
            {
                label: 'Water',
                data: Array.from({ length: 24 }, () => getRandomValue(10,15)),
    			backgroundColor: 'rgba(110,169,228,0.50)',
				borderColor: 'rgba(84,102,255,1.00)',
				borderWidth: 1,
				pointRadius: 1,
				fill: true,
				tension: 0.5,
            },
        ],
    },
    options: {
        responsive: true,
		maintainAspectRatio: false,
		plugins: {
			legend: {
				display: false // Set display to false to hide the legend
			}
		},
		scales: {
            y: {
                beginAtZero: false,
            },
        },
    },
});

const tempchart = new Chart(tempCtx, {
    type: 'line',
    data: {
        labels: Array.from({ length: 24 }, (_, i) => (i-24).toString()),
        datasets: [
            {
                label: 'Temperature',
                data: Array.from({ length: 24 }, () => getRandomValue(10,15)),
    			backgroundColor: 'rgba(238,196,30,0.50)',
				borderColor: 'rgba(242,255,83,1.00)',
				borderWidth: 1,
				pointRadius: 1,
				fill: true,
				tension: 0.5,
            },
        ],
    },
    options: {
        responsive: true,
		maintainAspectRatio: false,
		plugins: {
			legend: {
				display: false // Set display to false to hide the legend
			}
		},
		scales: {
            y: {
                beginAtZero: false,
            },
        },
    },
});

const humchart = new Chart(humCtx, {
    type: 'line',
    data: {
        labels: Array.from({ length: 24 }, (_, i) => (i-24).toString()),
        datasets: [
            {
                label: 'Humidity',
                data: Array.from({ length: 24 }, () => getRandomValue(20,40)),
    			backgroundColor: 'rgba(132,238,169,0.50)',
				borderColor: 'rgba(52,186,77,0.88)',
				borderWidth: 1,
				pointRadius: 1,
				fill: true,
				tension: 0.5,
			},
        ],
    },
    options: {
        responsive: true,
		maintainAspectRatio: false,
		plugins: {
			legend: {
				display: false // Set display to false to hide the legend
			}
		},
		scales: {
            y: {
                beginAtZero: false,
            },
        },
    },
});

const preschart = new Chart(presCtx, {
    type: 'line',
    data: {
        labels: Array.from({ length: 24 }, (_, i) => (i-24).toString()),
        datasets: [
            {
                label: 'Pressure',
                data: Array.from({ length: 24 }, () => getRandomValue(20,750)),
    			backgroundColor: 'rgba(255,119,62,0.50)',
				borderColor: 'rgba(225,117,140,1.00)',
				borderWidth: 1,
				pointRadius: 1,
				fill: true,
				tension: 0.5,
			},
        ],
    },
    options: {
        responsive: true,
		maintainAspectRatio: false,
		plugins: {
			legend: {
				display: false // Set display to false to hide the legend
			}
		},
		scales: {
            y: {
                beginAtZero: false,
            },
        },
    },
});

function onload(event) {
    initWebSocket();
}

function getValues(){
    websocket.send("getValues");
}

function initWebSocket() {
    console.log('Trying to open a WebSocket connection…');
    websocket = new WebSocket(gateway);
    websocket.onopen = onOpen;
    websocket.onclose = onClose;
    websocket.onmessage = onMessage;
}

function onOpen(event) {
    console.log('Connection opened');
    getValues();
}

function onClose(event) {
    console.log('Connection closed');
    setTimeout(initWebSocket, 2000);
}

function updateSliderPWM(element) {
    var sliderNumber = element.id.charAt(element.id.length-1);
    var sliderValue = document.getElementById(element.id).value;
    document.getElementById("sliderValue"+sliderNumber).innerHTML = sliderValue;
    console.log(sliderValue);
    websocket.send(sliderNumber+"s"+sliderValue.toString());
}

// Event handler for receiving messages from the server
function onMessage(event) {
 //   console.log(event.data);
      var data = JSON.parse(event.data);

      // Update the respective spans with the received data
      document.getElementById('timedate').textContent = data.timedate;
		var currentHour = parseInt(data.timedate.split(" ")[1].split(":")[0]);
		var currentMinute = parseInt(data.timedate.split(" ")[1].split(":")[1]);
      document.getElementById('rpm').textContent = data.rpm;
      document.getElementById('depth').textContent = data.depth;
      document.getElementById('speed').textContent = data.speed;
      document.getElementById('heading').textContent = data.heading;
      document.getElementById('windspeed').textContent = data.windspeed;
      document.getElementById('winddir').textContent = data.winddir;
      document.getElementById('longitude').textContent = data.longitude;
      document.getElementById('latitude').textContent = data.latitude;

		if (currentMinute !== lastMinute) {
  			lastMinute = currentMinute;	
			
			document.getElementById('watertemp').textContent = data.watertemp;
			var waterTemp = parseInt(data.watertemp, 10); 
    		waterchart.data.datasets[0].data.shift();
    		waterchart.data.datasets[0].data.push(waterTemp);
    		waterchart.update();		
			
      		document.getElementById('airtemp').textContent = data.airtemp;
			var airTemp = parseInt(data.airtemp, 10); 
    		tempchart.data.datasets[0].data.shift();
    		tempchart.data.datasets[0].data.push(airTemp);
    		tempchart.update();
	
	  		document.getElementById('humidity').textContent = data.humidity;
			var airHumidity = parseInt(data.humidity, 10); 
    		humchart.data.datasets[0].data.shift();
    		humchart.data.datasets[0].data.push(airHumidity);
    		humchart.update();

			document.getElementById('pressure').textContent = data.pressure;
			var airPressure = parseInt(data.pressure, 10); 
    		preschart.data.datasets[0].data.shift();
    		preschart.data.datasets[0].data.push(airPressure);
    		preschart.update();
		}
}
