  <div id="main">
    <div class="header">
      <h1><span class="esp">esp</span> link - Wifi Configuration</h1>
    </div>

    <div class="content pure-g">
      <div class="pure-u-12-24"><div class="card">
        <h1>Wifi State</h2>
        <div id="wifi-spinner" class="spinner spinner-small"></div>
        <table id="wifi-table" class="pure-table pure-table-horizontal" hidden><tbody>
        <tr><td>WiFi mode</td><td id="wifi-mode"></td></tr>
        <tr><td>Configured network</td><td id="wifi-ssid"></td></tr>
        <tr><td>Wifi status</td><td id="wifi-status"></td></tr>
        <tr><td>Wifi address</td><td id="wifi-ip"></td></tr>
        <tr><td>Wifi rssi</td><td id="wifi-rssi"></td></tr>
        <tr><td>Wifi phy</td><td id="wifi-phy"></td></tr>
        <tr><td>Wifi MAC</td><td id="wifi-mac"></td></tr>
        <tr><td colspan="2" id="wifi-warn"></td></tr>
        </tbody> </table>
      </div></div>
      <div class="pure-u-12-24"><div class="card">
        <h1>Wifi Association</h2>
        <p id="reconnect" style="color: #600" hidden></p>
        <form action="#" id="wifiform" class="pure-form pure-form-stacked">
        <!--form name="wifiform" action="connect.cgi" method="post"-->
          <legend>To connect to a WiFi network, please select one of the detected networks,
             enter the password, and hit the connect button...</legend>
          <label>Network SSID</label>
          <div id="aps">Scanning... <div class="spinner spinner-small"></div></div>
          <label>WiFi password, if applicable:</label>
          <input id="wifi-passwd" type="text" name="passwd" placeholder="password">
          <button id="connect-button" type="submit" class="pure-button button-primary">Connect!</button>
        </form>
      </div></div>
    </div>
  </div>
</div>

<script src="/ui.js"></script>
<script type="text/javascript">

var currAp = "";
var blockScan = 0;

function createInputForAp(ap) {
  if (ap.essid=="" && ap.rssi==0) return;

  var input = document.createElement("input");
  input.type = "radio";
  input.name = "essid";
  input.value=ap.essid;
  input.id   = "opt-" + ap.essid;
  if (currAp == ap.essid) input.checked = "1";

  var bars    = document.createElement("div");
  var rssiVal = -Math.floor(ap.rssi/51)*32;
  bars.className = "lock-icon";
  bars.style.backgroundPosition = "0px "+rssiVal+"px";

  var rssi = document.createElement("div");
  rssi.innerHTML = "" + ap.rssi +"dB";

  var encrypt = document.createElement("div");
  var encVal  = "-64"; //assume wpa/wpa2
  if (ap.enc == "0") encVal = "0"; //open
  if (ap.enc == "1") encVal = "-32"; //wep
  encrypt.className = "lock-icon";
  encrypt.style.backgroundPosition = "-32px "+encVal+"px";

  var label = document.createElement("div");
  label.innerHTML = ap.essid;

  var div = document.createElement("label");
  div.for = "opt-" + ap.essid;
  div.appendChild(input);
  div.appendChild(encrypt);
  div.appendChild(bars);
  div.appendChild(rssi);
  div.appendChild(label);
  return div;
}

function getSelectedEssid() {
  var e = document.forms.wifiform.elements;
  for (var i=0; i<e.length; i++) {
    if (e[i].type == "radio" && e[i].checked) return e[i].value;
  }
  return currAp;
}

var scanTimeout = null;
var scanReqCnt = 0;

function scanResult() {
  if (scanReqCnt > 60) {
    return scanAPs();
  }
  scanReqCnt += 1;
  ajaxJson('GET', "scan", function(data) {
      currAp = getSelectedEssid();
      if (data.result.inProgress == "0" && data.result.APs.length > 1) {
        $("#aps").innerHTML = "";
        var n = 0;
        for (var i=0; i<data.result.APs.length; i++) {
          if (data.result.APs[i].essid == "" && data.result.APs[i].rssi == 0) continue;
          $("#aps").appendChild(createInputForAp(data.result.APs[i]));
          n = n+1;
        }
        showNotification("Scan found " + n + " networks");
        var cb = $("#connect-button");
        cb.className = cb.className.replace(" pure-button-disabled", "");
        if (scanTimeout != null) clearTimeout(scanTimeout);
        scanTimeout = window.setTimeout(scanAPs, 20000);
      } else {
        window.setTimeout(scanResult, 1000);
      }
    }, function(s, st) {
      window.setTimeout(scanResult, 5000);
  });
}

function scanAPs() {
  if (blockScan) {
    scanTimeout = window.setTimeout(scanAPs, 1000);
    return;
  }
  scanTimeout = null;
  scanReqCnt = 0;
  ajaxReq('POST', "scan", function(data) {
    //showNotification("Wifi scan started");
    window.setTimeout(scanResult, 1000);
  }, function(s, st) {
    //showNotification("Wifi scan may have started?");
    window.setTimeout(scanResult, 1000);
  });
}

function getStatus() {
  ajaxJsonSpin("GET", "connstatus", function(data) {
      if (data.status == "idle" || data.status == "connecting") {
        $("#aps").innerHTML = "Connecting...";
        showNotification("Connecting...");
        window.setTimeout(getStatus, 1000);
      } else if (data.status == "got IP address") {
        var txt = "Connected! Got IP "+data.ip;
        showNotification(txt);
        showWifiInfo(data);
        blockScan = 0;

        var txt2 = "ESP Link will switch to STA-only mode in a few seconds";
        window.setTimeout(function() { showNotification(txt2); }, 4000);

        $("#reconnect").removeAttribute("hidden");
        $("#reconnect").innerHTML =
          "If you are in the same network, go to <a href=\"http://"+data.ip+
          "/\">"+data.ip+"</a>, else connect to network "+data.ssid+" first.";
      } else {
        blockScan = 0;
        showWarning("Connection failed: " + data.status + ", " + data.reason);
        $("#aps").innerHTML = 
          "Check password and selected AP. <a href=\"wifi.tpl\">Go Back</a>";
      }
    }, function(s, st) {
      //showWarning("Can't get status: " + st);
      window.setTimeout(getStatus, 2000);
    });
}

function changeWifiMode(m) {
  blockScan = 1;
  hideWarning();
  ajaxSpin("POST", "setmode?mode=" + m, function(resp) {
    showNotification("Mode changed");
    window.setTimeout(getWifiInfo, 100);
    blockScan = 0;
  }, function(s, st) {
    showWarning("Error changing mode: " + st);
    window.setTimeout(getWifiInfo, 100);
    blockScan = 0;
  });
}

function changeWifiAp(e) {
  e.preventDefault();
  var passwd = $("#wifi-passwd").value;
  var essid = getSelectedEssid();
  console.log("Posting form", "essid=" + essid, "pwd="+passwd);
  showNotification("Connecting to " + essid);
  var url = "connect?essid="+encodeURIComponent(essid)+"&passwd="+encodeURIComponent(passwd);

  hideWarning();
  $("#reconnect").setAttribute("hidden", "");
  $("#wifi-passwd").value = "";
  var cb = $("#connect-button");
  var cn = cb.className;
  cb.className += ' pure-button-disabled';
  blockScan = 1;
  ajaxSpin("POST", url, function(resp) {
      $("#spinner").removeAttribute('hidden'); // hack
      showNotification("Waiting for network change...");
      window.scrollTo(0, 0);
      window.setTimeout(getStatus, 2000);
    }, function(s, st) {
      showWarning("Error switching network: "+st);
      cb.className = cn;
      window.setTimeout(scanAPs, 1000);
    });
}

window.onload=function(e) {
  getWifiInfo();
  $("#wifiform").onsubmit = changeWifiAp;
  scanTimeout = window.setTimeout(scanAPs, 500);
};
</script>
</body></html>
