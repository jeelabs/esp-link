  <div id="main">
    <div class="header">
      <h1><span class="esp">esp</span> link - Wifi Configuration</h1>
    </div>

    <div class="content pure-g">
      <div class="pure-u-12-24"><div class="card">
        <h1>Wifi State</h2>
        <table class="pure-table pure-table-horizontal"><tbody>
        <tr><td>WiFi mode</td><td id="wifi-mode"></td></tr>
        <tr><td>Configured network</td><td id="wifi-ssid"></td></tr>
        <tr><td>Wifi status</td><td id="wifi-status"></td></tr>
        <tr><td>Wifi rssi</td><td id="wifi-rssi"></td></tr>
        <tr><td>Wifi phy</td><td id="wifi-phy"></td></tr>
        <tr><td colspan="2" id="wifi-warn"></td></tr>
        </tbody> </table>
      </div></div>
      <div class="pure-u-12-24"><div class="card">
        <h1>Wifi Association</h2>
        <form action="#" id="wifiform" class="pure-form pure-form-stacked">
        <!--form name="wifiform" action="connect.cgi" method="post"-->
          <legend>To connect to a WiFi network, please select one of the detected networks,
             enter the password, and hit the connect button...</legend>
          <label>Network SSID</label>
          <div id="aps">Scanning...</div>
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

var scan_xhr = j();
function scanAPs() {
  scan_xhr.open("GET", "wifiscan");
  scan_xhr.onreadystatechange = function() {
    if (scan_xhr.readyState == 4 && scan_xhr.status >= 200 && scan_xhr.status < 300) {
      var data = JSON.parse(scan_xhr.responseText);
      currAp = getSelectedEssid();
      if (data.result.inProgress == "0" && data.result.APs.length > 1) {
        $("#aps").innerHTML = "";
        for (var i=0; i<data.result.APs.length; i++) {
          if (data.result.APs[i].essid == "" && data.result.APs[i].rssi == 0) continue;
          $("#aps").appendChild(createInputForAp(data.result.APs[i]));
        }
        //window.setTimeout(scanAPs, 10000);
      } else {
        window.setTimeout(scanAPs, 1000);
      }
    }
  }
  scan_xhr.send();
}

function getWifiInfo() {
  var xhr = j();
  xhr.open("GET", "info");
  xhr.onreadystatechange = function() {
    if (xhr.readyState != 4) { return; }
    if (xhr.status >= 200 && xhr.status < 300) {
      var data = JSON.parse(xhr.responseText);
      Object.keys(data).forEach(function(v) {
        el = document.getElementById("wifi-" + v);
        if (el != null) el.innerHTML = data[v];
      });
      currAp = data.ssid;
    } else {
      window.setTimeout(getWifiInfo, 1000);
    }
  }
  xhr.send();
}

function getStatus() {
  var xhr = j();
  xhr.open("GET", "connstatus");
  xhr.onreadystatechange = function() {
    if (xhr.readyState != 4) { return; }
    if (xhr.status >= 200 && xhr.status < 300) {
      var data = JSON.parse(xhr.responseText);
      if (data.status == "idle" || data.status == "connecting") {
        $("#aps").innerHTML = "Connecting...";
        window.setTimeout(getStatus, 1000);
      } else if (data.status == "got IP address") {
        $("#aps").innerHTML="Connected! Got IP "+data.ip+ ".<br/>" +
          "If you're in the same network, you can access it <a href=\"http://"+data.ip+
          "/\">here</a>.<br/>ESP Link will switch to STA-only mode in a few seconds.";
      } else {
        $("#aps").innerHTML="Oops: " + data.status + ". Reason: " + data.reason +
          "<br/>Check password and selected AP.<br/><a href=\"wifi.tpl\">Go Back</a>";
      }
    } else {
      window.setTimeout(getStatus, 2000);
    }
  }
  xhr.send();
}

function changeWifiAp(e) {
  e.preventDefault();
  var xhr = j();
  var passwd = $("#wifi-passwd").value;
  var essid = getSelectedEssid();
  console.log("Posting form", "essid=" + essid, "pwd="+passwd);
  xhr.open("POST", "connect");
  xhr.onreadystatechange = function() {
    if (xhr.readyState != 4) { return; }
    if (xhr.status >= 200 && xhr.status < 300) {
      window.setTimeout(getStatus, 2000);
    } else {
      window.setTimeout(scanAPs, 1000);
    }
  }
  xhr.setRequestHeader("Content-type", "application/x-form-urlencoded");
  xhr.send("essid="+encodeURIComponent(essid)+"&passwd="+encodeURIComponent(passwd));
}

window.onload=function(e) {
  getWifiInfo();
  $("#wifiform").onsubmit = changeWifiAp;
  window.setTimeout(scanAPs, 500);
};
</script>
</body></html>
