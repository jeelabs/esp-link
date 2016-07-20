var specials = [];
specials["ap_ssid"] = "SSID name";
specials["ap_password"] = "PASSWORD";
specials["ap_maxconn"] = "Max Connections number";
specials["ap_beacon"] = "Beacon Interval";

function changeWifiMode(m) {
  blockScan = 1;
  hideWarning();
  ajaxSpin("POST", "/wifi/setmode?mode=" + m, function(resp) {
    showNotification("Mode changed");
    window.setTimeout(getWifiInfo, 100);
    blockScan = 0;
  }, function(s, st) {
    showWarning("Error changing mode: " + st);
    window.setTimeout(getWifiInfo, 100);
    blockScan = 0;
  });
}

function changeApSettings(e) {
  e.preventDefault();
  var url = "/wifi/apchange?100=1";
  var i, inputs = document.querySelectorAll("#" + e.target.id + " input,select");
  for (i = 0; i < inputs.length; i++) {
    if (inputs[i].type == "checkbox") {
      var val = (inputs[i].checked) ? 1 : 0;
      url += "&" + inputs[i].name + "=" + val;
    } else {
      var clean = inputs[i].value.replace(/[^!-~]/g, "");
      var comp = clean.localeCompare(inputs[i].value);
      if ( comp != 0 ){
        showWarning("Invalid characters in " + specials[inputs[i].name]);
        return;
      }
      url += "&" + inputs[i].name + "=" + clean;
    }
  };

  hideWarning();
  var n = e.target.id.replace("-form", "");
  var cb = $("#" + n + "-button");
  addClass(cb, "pure-button-disabled");
  ajaxSpin("POST", url, function (resp) {
    showNotification(n + " updated");
    removeClass(cb, "pure-button-disabled");
    window.setTimeout(getWifiInfo, 100);
  }, function (s, st) {
    showWarning(st);
    removeClass(cb, "pure-button-disabled");
    window.setTimeout(fetchApSettings, 2000);
  });
}

function displayApSettings(data) {
  Object.keys(data).forEach(function (v) {
    el = $("#" + v);
    if (el != null) {
      if (el.nodeName === "INPUT") el.value = data[v];
      else el.innerHTML = data[v];
      return;
    }

    el = document.querySelector('input[name="' + v + '"]');
    if (el == null)
      el = document.querySelector('select[name="' + v + '"]');

    if (el != null) {
      if (el.type == "checkbox") {
        el.checked = data[v] == "enabled";
      } else el.value = data[v];
    }
  });

  $("#AP_Settings-spinner").setAttribute("hidden", "");
  $("#AP_Settings-form").removeAttribute("hidden");
  showWarning("Don't modify SOFTAP parameters with active connections");
  window.setTimeout(hideWarning(), 2000);
}

function fetchApSettings() {
  ajaxJson("GET", "/wifi/apinfo", displayApSettings, function () {
    window.setTimeout(fetchApSettings, 1000);
  });
}

function doApAdvanced() {
  $('#AP_Settings-on').removeAttribute('hidden');
  $("#AP_Settings-off").setAttribute("hidden", "");
  $("#AP_Settings-roff").removeAttribute("checked");
}

function undoApAdvanced(){
  $("#AP_Settings-on").setAttribute("hidden", "");
  $("#AP_Settings-off").removeAttribute("hidden");
  $("#AP_Settings-roff").setAttribute("checked", "");
}

onLoad(function() {
  // Show info about AP	
  getWifiInfo();
  // Fetch actual settings
  fetchApSettings();
  // Hide advanced settings
  undoApAdvanced();
  document.ap_form.ap.value='off';
  bnd($("#AP_Settings-ron"), "click", doApAdvanced);
  bnd($("#AP_Settings-roff"), "click", undoApAdvanced); 
  bnd($("#AP_Settings-form"), "submit", changeApSettings);
});
