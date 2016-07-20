onLoad(function() {
  fetchMqtt();
  bnd($("#mqtt-form"), "submit", changeMqtt);
  bnd($("#mqtt-status-form"), "submit", changeMqttStatus);
});

//===== MQTT cards

function changeMqtt(e) {
  e.preventDefault();
  var url = "mqtt?1=1";
  var i, inputs = document.querySelectorAll('#mqtt-form input');
  for (i = 0; i < inputs.length; i++) {
    if (inputs[i].type != "checkbox")
      url += "&" + inputs[i].name + "=" + inputs[i].value;
  };

  hideWarning();
  var cb = $("#mqtt-button");
  addClass(cb, 'pure-button-disabled');
  ajaxSpin("POST", url, function (resp) {
    showNotification("MQTT updated");
    removeClass(cb, 'pure-button-disabled');
  }, function (s, st) {
    showWarning("Error: " + st);
    removeClass(cb, 'pure-button-disabled');
    window.setTimeout(fetchMqtt, 100);
  });
}

function displayMqtt(data) {
  Object.keys(data).forEach(function (v) {
    el = $("#" + v);
    if (el != null) {
      if (el.nodeName === "INPUT") el.value = data[v];
      else el.innerHTML = data[v];
      return;
    }
    el = document.querySelector('input[name="' + v + '"]');
    if (el != null) {
      if (el.type == "checkbox") el.checked = data[v] > 0;
      else el.value = data[v];
    }
  });
  $("#mqtt-spinner").setAttribute("hidden", "");
  $("#mqtt-status-spinner").setAttribute("hidden", "");
  $("#mqtt-form").removeAttribute("hidden");
  $("#mqtt-status-form").removeAttribute("hidden");

  var i, inputs = $("input");
  for (i = 0; i < inputs.length; i++) {
    if (inputs[i].type == "checkbox")
      inputs[i].onclick = function () { setMqtt(this.name, this.checked) };
  }
}

function fetchMqtt() {
  ajaxJson("GET", "/mqtt", displayMqtt, function () {
    window.setTimeout(fetchMqtt, 1000);
  });
}

function changeMqttStatus(e) {
  e.preventDefault();
  var v = document.querySelector('input[name="mqtt-status-topic"]').value;
  ajaxSpin("POST", "/mqtt?mqtt-status-topic=" + v, function () {
    showNotification("MQTT status settings updated");
  }, function (s, st) {
    showWarning("Error: " + st);
    window.setTimeout(fetchMqtt, 100);
  });
}

function setMqtt(name, v) {
  ajaxSpin("POST", "/mqtt?" + name + "=" + (v ? 1 : 0), function () {
    var n = name.replace("-enable", "");
    showNotification(n + " is now " + (v ? "enabled" : "disabled"));
  }, function () {
    showWarning("Enable/disable failed");
    window.setTimeout(fetchMqtt, 100);
  });
}