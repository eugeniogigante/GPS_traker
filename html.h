// HTML template come stringa letterale
const char* MAP_HTML = R"rawliteral(
<!DOCTYPE html>
<html>
<head>
  <title>GPS Tracker Live Map</title>
  <style>
    #map { height: 100vh; width: 100%; }
    body { margin: 0; padding: 0; }
    .controls {
      position: absolute;
      top: 10px;
      left: 10px;
      background: white;
      padding: 5px;
      z-index: 1000;
      border-radius: 5px;
      box-shadow: 0 2px 5px rgba(0,0,0,0.3);
    }
    .info-panel {
      position: absolute;
      bottom: 10px;
      left: 10px;
      background: white;
      padding: 10px;
      z-index: 1000;
      border-radius: 5px;
      box-shadow: 0 2px 5px rgba(0,0,0,0.3);
    }
  </style>
  <script src="https://maps.googleapis.com/maps/api/js?key=MyAPIGoogleKey"></script>
</head>
<body>
  <div id="map"></div>
  <div class="controls">
    <a href="/">Home</a> | 
    <span id="lastUpdate">Ultimo aggiornamento: nessuno</span>
  </div>
  <div class="info-panel">
    <div>Latitudine: <span id="infoLat">-</span></div>
    <div>Longitudine: <span id="infoLng">-</span></div>
    <div>Satelliti: <span id="infoSat">-</span></div>
    <div>Velocità: <span id="infoSpeed">-</span> km/h</div>
  </div>
</body>
</html>
)rawliteral";

const char* CONFIG_HTML = R"rawliteral(
<!DOCTYPE html>
<html>
<head>
  <title>Cloud Configuration</title>
  <style>
    body { font-family: Arial, sans-serif; margin: 20px; }
    .config-form { max-width: 600px; margin: 0 auto; }
    .form-group { margin-bottom: 15px; }
    label { display: block; margin-bottom: 5px; font-weight: bold; }
    input[type="text"], input[type="password"] {
      width: 100%; padding: 8px; border: 1px solid #ddd; border-radius: 4px;
    }
    button { 
      background-color: #4CAF50; color: white; padding: 10px 15px; 
      border: none; border-radius: 4px; cursor: pointer; 
    }
    button:hover { background-color: #45a049; }
    .status { margin-top: 20px; padding: 10px; border-radius: 4px; }
    .success { background-color: #dff0d8; color: #3c763d; }
    .error { background-color: #f2dede; color: #a94442; }
  </style>
</head>
<body>
  <div class="config-form">
    <h1>Cloud Service Configuration</h1>
    <form id="configForm">
      <div class="form-group">
        <label for="firebaseHost">Firebase Host:</label>
        <input type="text" id="firebaseHost" name="firebaseHost" value="%FIREBASE_HOST%">
      </div>
      <div class="form-group">
        <label for="firebaseKey">Firebase Key:</label>
        <input type="password" id="firebaseKey" name="firebaseKey" value="%FIREBASE_KEY%">
      </div>
      <div class="form-group">
        <label for="serverApi">Server API:</label>
        <input type="text" id="serverApi" name="serverApi" value="%SERVER_API%">
      </div>
      <div class="form-group">
        <label for="thingspeakKey">ThingSpeak Key:</label>
        <input type="password" id="thingspeakKey" name="thingspeakKey" value="%THINGSPEAK_KEY%">
      </div>
      <div class="form-group">
        <label for="Token">Token:</label>
        <input type="password" id="Token" name="Token" value="%TOKEN%">
      </div>
      <div class="form-group">
        <label for="apn">APN:</label>
        <input type="text" id="apn" name="apn" value="%APN%">
      </div>
      <button type="submit">Save Configuration</button>
    </form>
    <div id="statusMessage" class="status" style="display: none;"></div>
  </div>
  <script>
    document.getElementById('configForm').addEventListener('submit', function(e) {
      e.preventDefault();
      
      const formData = new FormData(this);
      const params = new URLSearchParams();
      for (const [key, value] of formData.entries()) {
        params.append(key, value);
      }
      
      fetch('/config', {
        method: 'POST',
        body: params
      })
      .then(response => response.text())
      .then(text => {
        const statusDiv = document.getElementById('statusMessage');
        statusDiv.style.display = 'block';
        if (text === 'OK') {
          statusDiv.className = 'status success';
          statusDiv.textContent = 'Configuration saved successfully!';
        } else {
          statusDiv.className = 'status error';
          statusDiv.textContent = 'Error saving configuration: ' + text;
        }
      })
      .catch(error => {
        const statusDiv = document.getElementById('statusMessage');
        statusDiv.style.display = 'block';
        statusDiv.className = 'status error';
        statusDiv.textContent = 'Error: ' + error;
      });
    });
  </script>
</body>
</html>
)rawliteral";


// Modifica il template HTML cambiando il meta refresh e aggiungendo un timer visivo
const char* MAP_HTML_TEMPLATE = R"rawliteral(
<!DOCTYPE html>
<html>
<head>
  <title>GPS Tracker Live Map</title>
  <meta http-equiv="refresh" content="60">
  <style>
    #map { height: 100vh; width: 100%; }
    body { margin: 0; padding: 0; }
    .controls {
      position: absolute;
      top: 10px;
      left: 10px;
      background: white;
      padding: 10px;
      z-index: 1000;
      border-radius: 5px;
      box-shadow: 0 2px 5px rgba(0,0,0,0.3);
      font-family: Arial, sans-serif;
    }
    .info-panel {
      position: absolute;
      bottom: 10px;
      left: 10px;
      background: rgba(255,255,255,0.9);
      padding: 10px;
      z-index: 1000;
      border-radius: 5px;
      box-shadow: 0 2px 5px rgba(0,0,0,0.3);
      font-family: Arial, sans-serif;
    }
    .info-row { margin: 5px 0; }
    .label { font-weight: bold; display: inline-block; width: 80px; }
    .refresh-timer {
      display: inline-block;
      margin-left: 15px;
      color: #666;
    }
  </style>
  <script src="https://maps.googleapis.com/maps/api/js?key=MyAPIGoogleKey"></script>
  <script>
    var map;
    var marker;
    var lastUpdate = "%LAST_UPDATE%";
    var timeLeft = 60;
    
    function initMap() {
      map = new google.maps.Map(document.getElementById('map'), {
        zoom: 15,
        center: {lat: %LAT%, lng: %LNG%},
        mapTypeId: 'hybrid'
      });

      marker = new google.maps.Marker({
        position: {lat: %LAT%, lng: %LNG%},
        map: map,
        title: "Current Position",
        icon: {
          path: google.maps.SymbolPath.CIRCLE,
          scale: 10,
          fillColor: "#4285F4",
          fillOpacity: 1,
          strokeWeight: 2,
          strokeColor: "#FFFFFF"
        }
      });

      // Auto-centra la mappa quando si ridimensiona la finestra
      window.addEventListener('resize', function() {
        map.setCenter(marker.getPosition());
      });
      
      // Avvia il contatore per il refresh
      startRefreshTimer();
    }
    
    function startRefreshTimer() {
      var timerElement = document.getElementById('refreshTimer');
      var timer = setInterval(function() {
        timeLeft--;
        timerElement.textContent = "Aggiornamento in: " + timeLeft + "s";
        
        if (timeLeft <= 0) {
          clearInterval(timer);
          timerElement.textContent = "Aggiornamento in corso...";
        }
      }, 1000);
    }
    
    function updateInfoPanel() {
      document.getElementById('infoLat').textContent = "%LAT%";
      document.getElementById('infoLng').textContent = "%LNG%";
      document.getElementById('infoSat').textContent = "%SAT%";
      document.getElementById('infoSpeed').textContent = "%SPEED%";
      document.getElementById('infoAcc').textContent = "%ACC%";
      document.getElementById('lastUpdate').textContent = "Ultimo aggiornamento: " + lastUpdate;
    }
    
    window.onload = function() {
      initMap();
      updateInfoPanel();
    };
  </script>
</head>
<body>
  <div id="map"></div>
  <div class="controls">
    <a href="/">Home</a> | 
    <span id="lastUpdate"></span>
    <span id="refreshTimer" class="refresh-timer">Aggiornamento in: 60s</span>
  </div>
  <div class="info-panel">
    <div class="info-row"><span class="label">Latitudine:</span> <span id="infoLat">%LAT%</span></div>
    <div class="info-row"><span class="label">Longitudine:</span> <span id="infoLng">%LNG%</span></div>
    <div class="info-row"><span class="label">Satelliti:</span> <span id="infoSat">%SAT%</span></div>
    <div class="info-row"><span class="label">Velocità:</span> <span id="infoSpeed">%SPEED%</span> km/h</div>
    <div class="info-row"><span class="label">Accuratezza:</span> <span id="infoAcc">%ACC%</span> m</div>
  </div>
</body>
</html>
)rawliteral";