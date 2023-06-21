// Complete project details: https://randomnerdtutorials.com/esp32-web-server-websocket-sliders/

var gateway = `ws://${window.location.hostname}/ws`;
var websocket;
window.addEventListener('load', onload);

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
    console.log(event.data);
      var data = JSON.parse(event.data);

      // Update the respective spans with the received data
      document.getElementById('timedate').textContent = data.timedate;
      document.getElementById('rpm').textContent = data.rpm;
      document.getElementById('depth').textContent = data.depth;
      document.getElementById('speed').textContent = data.speed;
      document.getElementById('heading').textContent = data.heading;
      document.getElementById('windspeed').textContent = data.windspeed;
      document.getElementById('winddir').textContent = data.winddir;
      document.getElementById('longitude').textContent = data.longitude;
      document.getElementById('latitude').textContent = data.latitude;
      document.getElementById('watertemp').textContent = data.watertemp;
      document.getElementById('humidity').textContent = data.humidity;
      document.getElementById('pressure').textContent = data.pressure;
      document.getElementById('airtemp').textContent = data.airtemp;
}
