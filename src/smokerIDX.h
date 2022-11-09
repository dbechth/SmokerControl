const char smokerIDX[] PROGMEM = R"=====(
<!DOCTYPE html>
<html>
<script src="https://cdnjs.cloudflare.com/ajax/libs/Chart.js/2.9.4/Chart.js"></script>
<head>
<title>BBQ Controller</title>
</head>
<body style = "background-color:#cccccc; font-family: Arial, Helvetica, Sans-Serif; Color:#000088; font-size:40px">
<h1 align = "center">&#127830;BBQ&#127830; <br>
CONTROLLER</h1>

<div align = "center">Smoker Temp : <span id = "smokerTemp"> ? </span>&#176;F</div>
<div align = "center">Damper: <span id = "damperPCT"> ? </span>% </div>
<div align = "center">Cold Transitions: <span id = "transIdleToCool"> ? </span>% </div>
<div align = "center">Hot Transitions: <span id = "transIdleToHeat"> ? </span>% </div>
<canvas id="SmokerChart" style="width:100%"></canvas>

<div align = "center"> <button type = "button" style = "font-size:40px; height:100px; width:25%; background-color:#CB2717" id = "autoON" onClick = "setBtn(this.id)">Auto</button>
<button type = "button" style = "font-size:40px; height:100px; width:25%; background-color:#CB2717" id = "manualON" onClick = "setBtn(this.id)">Manual</button></div>
<div align = "center"> <button type = "button" style = "font-size:40px; height:100px; width:25%; background-color:#CB2717" id = "startup" onClick = "setBtn(this.id)">Startup</button>
<button type = "button" style = "font-size:40px; height:100px; width:25%; background-color:#CB2717" id = "shutdown" onClick = "setBtn(this.id)">Shutdown</button></div>
<br>
<div align = "center">Temperature Set Point
<br>
<input type = "number" style = "font-size:40px; height:80px; width:20%" id = "setPointF" value = ""/>
<button type = "button" style = "font-size:40px; height:80px; width:20%" onClick = "set(&quot;setPointF&quot;)">Set</button></div>
<br>
<div align = "center">Deadband
<br>
<input type = "number" style = "font-size:40px; height:80px; width:20%" id = "setPointDeadband" value = ""/>
<button type = "button" style = "font-size:40px; height:80px; width:20%" onClick = "set(&quot;setPointDeadband&quot;)">Set</button></div>
<br>
<div align = "center">Hot Damper Percent
<br>
<input type = "number" style = "font-size:40px; height:80px; width:20%" id = "hotPCT" value = ""/>
<button type = "button" style = "font-size:40px; height:80px; width:20%" onClick = "set(&quot;hotPCT&quot;)">Set</button></div>
<br>
<div align = "center">Idle Damper Percent
<br>
<input type = "number" style = "font-size:40px; height:80px; width:20%" id = "idlePCT" value = ""/>
<button type = "button" style = "font-size:40px; height:80px; width:20%" onClick = "set(&quot;idlePCT&quot;)">Set</button></div>
<br>
<div align = "center">Cold Damper Percent
<br>
<input type = "number" style = "font-size:40px; height:80px; width:20%" id = "coldPCT" value = ""/>
<button type = "button" style = "font-size:40px; height:80px; width:20%" onClick = "set(&quot;coldPCT&quot;)">Set</button></div>
<br>
<div align = "center">Manual Damper Percent
<br>
<input type = "number" style = "font-size:40px; height:80px; width:20%" id = "manualPCT" value = ""/>
<button type = "button" style = "font-size:40px; height:80px; width:20%" onClick = "set(&quot;manualPCT&quot;)">Set</button></div>
<br>
<div align = "center">Min Servo Pulse Width
<br>
<input type = "number" style = "font-size:40px; height:80px; width:20%" id = "minPW" value = ""/>
<button type = "button" style = "font-size:40px; height:80px; width:20%" onClick = "set(&quot;minPW&quot;)">Set</button></div>
<br>
<div align = "center">Max Servo Pulse Width
<br>
<input type = "number" style = "font-size:40px; height:80px; width:20%" id = "maxPW" value = ""/>
<button type = "button" style = "font-size:40px; height:80px; width:20%" onClick = "set(&quot;maxPW&quot;)">Set</button></div>
<br>
<div align = "center">Preheat Offset
<br>
<input type = "number" style = "font-size:40px; height:80px; width:20%" id = "preheatOffset" value = ""/>
<button type = "button" style = "font-size:40px; height:80px; width:20%" onClick = "set(&quot;preheatOffset&quot;)">Set</button></div>
<br>
<div align = "center">Auto Idle Tune Threshold
<br>
<input type = "number" style = "font-size:40px; height:80px; width:20%" id = "autoIdleTuneThreshold" value = ""/>
<button type = "button" style = "font-size:40px; height:80px; width:20%" onClick = "set(&quot;autoIdleTuneThreshold&quot;)">Set</button></div>
<br>
<div align = "center">Type ( reset ) to reset defaults 
<br>
<input type = "text" style = "font-size:40px; height:80px; width:20%" id = "resetDefaults" value = ""/>
<button type = "button" style = "font-size:40px; height:80px; width:20%" onClick = "set(&quot;resetDefaults&quot;)">Set</button></div>

<script>


var smokerChart;
var lastIndex = -1;
setInterval(function() {
	getBtn("autoON");
	getBtn("manualON");
	getBtn("shutdown");
	getBtn("startup");
	get("smokerTemp");
	get("transIdleToCool");
	get("transIdleToHeat");
	get("damperPCT");
	getLastChartData("lastchartdata");
}, 5000);

function addLastData(jsonfile) {
	var obj = JSON.parse(jsonfile);

	var xValues = obj.SmokerData.map(function(e) {
	   return e.index;
	});

	if(lastIndex != xValues[0]){
	var temperatureData = obj.SmokerData.map(function(e) {
	   return e.temperature;
	});
	var damperPCTData = obj.SmokerData.map(function(e) {
	   return e.damperPCT;
	});

	var setpointData = obj.SmokerData.map(function(e) {
	   return e.setpoint;
	});
    smokerChart.data.labels.push(xValues[0]);
    smokerChart.data.datasets[0].data.push(temperatureData[0]);
    smokerChart.data.datasets[1].data.push(damperPCTData[0]);
    smokerChart.data.datasets[2].data.push(setpointData[0]);
    smokerChart.update();	
	lastIndex = xValues[0];
	}


}

function addData(jsonfile) {
	var obj = JSON.parse(jsonfile);
	var temperatureData = obj.SmokerData.map(function(e) {
	   return e.temperature;
	});
	var damperPCTData = obj.SmokerData.map(function(e) {
	   return e.damperPCT;
	});

	var setpointData = obj.SmokerData.map(function(e) {
	   return e.setpoint;
	});

	var xValues = obj.SmokerData.map(function(e) {
	   return e.index;
	});
	lastIndex = xValues[xValues.length-1];

var ctx = document.getElementById("SmokerChart");
	
smokerChart = new Chart(ctx, {
  type: "line",
data: {
    labels: xValues,
    datasets: [{
	  data: temperatureData,
	  label: 'Temperature',
      borderColor: "red",
      fill: false,
      yAxisID: 'A',
    },{
	  data: damperPCTData,
	  label: 'Damper Percent',
      borderColor: "green",
      fill: false,
      yAxisID: 'B',
    },{
	  data: setpointData,
	  label: 'Setpoint',
      borderColor: "blue",
      fill: false,
      yAxisID: 'A',
    }]
  },
   options: {
    scales: {
      yAxes: [{
        id: 'A',
        type: 'linear',
        position: 'left',
      }, {
        id: 'B',
        type: 'linear',
        position: 'right',
        ticks: {
          max: 100,
          min: 0
        }
      }]
    }
  }
}
);
}

function set(id) {
	var xhttp = new XMLHttpRequest();
	var value = document.getElementById(id).value;
	document.getElementById(id).value = "";
	xhttp.onreadystatechange = function() {
		if (this.readyState == 4 && this.status == 200) {
			get(id);
		}
	};
	xhttp.open("GET", "set?" + id + "=" + value, true);
	xhttp.send();

}
function get(id) {
	var xhttp = new XMLHttpRequest();
	xhttp.onreadystatechange = function() {
		if (this.readyState == 4 && this.status == 200) {
			document.getElementById(id).innerHTML = this.responseText;
			document.getElementById(id).value = this.responseText;
		}
	};
	xhttp.open("GET", "get?" + id, true);
	xhttp.send();
}
function getChartData(id) {
	var xhttp = new XMLHttpRequest();
	xhttp.onreadystatechange = function() {
		if (this.readyState == 4 && this.status == 200) {
			addData(this.responseText);
		}
	};
	xhttp.open("GET", "get?" + id, true);
	xhttp.send();
}
function getLastChartData(id) {
	var xhttp = new XMLHttpRequest();
	xhttp.onreadystatechange = function() {
		if (this.readyState == 4 && this.status == 200) {
			addLastData(this.responseText);
		}
	};
	xhttp.open("GET", "get?" + id, true);
	xhttp.send();
}

function setBtn(id) {
	var xhttp = new XMLHttpRequest();
	var value = document.getElementById(id).value;
	document.getElementById(id).style.backgroundColor = "#CB2717";
	xhttp.onreadystatechange = function() {
		if (this.readyState == 4 && this.status == 200) {
			getBtn("autoON");
			getBtn("manualON");
			getBtn("shutdown");
			getBtn("startup");
		}
	};
	xhttp.open("GET", "set?" + id, true);
	xhttp.send();

}
function getBtn(id) {
	var xhttp = new XMLHttpRequest();
	xhttp.onreadystatechange = function() {
		if (this.readyState == 4 && this.status == 200) {
			if (this.responseText == "0") {
				document.getElementById(id).style.backgroundColor = "#CFCFCF";
			}
			else if (this.responseText == "1") {
				document.getElementById(id).style.backgroundColor = "#61CB17";
			}
			else {
				document.getElementById(id).style.backgroundColor = "#CB2717";
			}
		}
	};
	xhttp.open("GET", "get?" + id, true);
	xhttp.send();
}

getChartData("chartdata");
get("smokerTemp");
get("damperPCT");
get("transIdleToCool");
get("transIdleToHeat");
get("setPointF");
get("idlePCT");
get("minPW");
get("maxPW");
get("preheatOffset");
get("hotPCT");
get("coldPCT");
get("manualPCT");
get("setPointDeadband");
get("autoIdleTuneThreshold");
getBtn("autoON");
getBtn("manualON");
getBtn("shutdown");
getBtn("startup");

</script>
</body>
</html>
)=====";
