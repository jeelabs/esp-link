  <div id="main">
    <div class="header">
      <h1><span class="esp">esp</span> link</h1>
      <h2 id="version"></h2>
    </div>

		<div class="content">
			<div class="pure-g">
				<div class="pure-u-24-24"><div class="card">
					<p>The ESP Link bridges the ESP8266 serial port to Wifi and it can
					program microcontrollers over the serial port, in particular Arduinos, AVRs, and
					NXP's LPC800-series ARM processors.</p>
				</div></div>
			</div>
			<div class="pure-g">
				<div class="pure-u-12-24"><div class="card">
					<h1>Wifi summary</h2>
					<div id="wifi-spinner" class="spinner spinner-small"></div>
					<table id="wifi-table" class="pure-table pure-table-horizontal" hidden><tbody>
					<tr><td>WiFi mode</td><td id="wifi-mode"></td></tr>
					<tr><td>Configured network</td><td id="wifi-ssid"></td></tr>
					<tr><td>Wifi status</td><td id="wifi-status"></td></tr>
					<tr><td>Wifi address</td><td id="wifi-ip"></td></tr>
					</tbody> </table>
				</div></div>
				<div class="pure-u-12-24"><div class="card">
					<h1>Pin assignment</h2>
					<legend>Select one of the following signal/pin assignments to match your hardware</legend>
					<fieldset class='radios' id='pin-mux'>
						<div class="spinner spinner-small"></div>
					</fieldset>
				</div></div>
			</div>
    </div>
  </div>
</div>

<script src="ui.js"></script>
<script type="text/javascript">

var currPin;
// pin={reset:12, isp:13, LED_conn:0, LED_ser:2}
function createInputForPin(pin) {
  var input = document.createElement("input");
  input.type = "radio";
  input.name = "pins";
  input.data = pin.name;
	input.className = "pin-input";
  input.value= pin.value;
  input.id   = "opt-" + pin.value;
  if (currPin == pin.name) input.checked = "1";

	var descr = m('<label for="opt-'+pin.value+'"><b>'+pin.name+":</b>"+pin.descr+"</label>");
	descr.for = "opt-" + pin.value;
	var div = document.createElement("div");
	div.appendChild(input);
	div.appendChild(descr);
	return div;
}

function displayPins(resp) {
	var po = $("#pin-mux");
	po.innerHTML = "";
	currPin = resp.curr;
	resp.map.forEach(function(v) {
		po.appendChild(createInputForPin(v));
	});
	var i, inputs = $(".pin-input");
	for (i=0; i<inputs.length; i++) {
		inputs[i].onclick = function() { setPins(this.value, this.data) };
	};
}

function fetchPins() {
  ajaxJson("GET", "/pins", displayPins, function() {
		window.setTimeout(fetchPins, 1000);
	});
}

function setPins(v, name) {
  ajaxSpin("POST", "/pins?map="+v, function() {
		showNotification("Pin assignment changed to " + name);
	}, function() {
		showNotification("Pin assignment change failed");
		window.setTimeout(fetchPins, 100);
	});
}

window.onload=function(e) {
	fetchPins();
  getWifiInfo();
};
</script>
</body></html>
