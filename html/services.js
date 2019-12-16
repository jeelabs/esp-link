function changeServices(e) {
  e.preventDefault();
  var url = "services/update?1=1";
  var i, inputs = document.querySelectorAll("#" + e.target.id + " input,select");
  for (i = 0; i < inputs.length; i++) {
    if (inputs[i].type == "checkbox") {
      if (inputs[i].name.slice(-6) == "enable")
        continue;
      var val = (inputs[i].checked) ? 1 : 0;
      url += "&" + inputs[i].name + "=" + val;
    }
    else
      url += "&" + inputs[i].name + "=" + inputs[i].value;
  };

  hideWarning();
  var n = e.target.id.replace("-form", "");
  var cb = $("#" + n + "-button");
  addClass(cb, "pure-button-disabled");
  ajaxSpin("POST", url, function (resp) {
    showNotification(n + " updated");
    removeClass(cb, "pure-button-disabled");
  }, function (s, st) {
    showWarning("Error: " + st);
    removeClass(cb, "pure-button-disabled");
    window.setTimeout(fetchServices, 100);
  });
}

function displayServices(data) {
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

  $("#syslog-spinner").setAttribute("hidden", "");
  $("#sntp-spinner").setAttribute("hidden", "");
  $("#mdns-spinner").setAttribute("hidden", "");

  if (data.syslog_host !== undefined) {
    $("#Syslog-form").removeAttribute("hidden");
  } else {
    // syslog disabled...
    $("#Syslog-form").parentNode.setAttribute("hidden", "");
  }
  $("#SNTP-form").removeAttribute("hidden");
  $("#mDNS-form").removeAttribute("hidden");

  var i, inputs = $("input");
  for (i = 0; i < inputs.length; i++) {
    if (inputs[i].name == "mdns_enable") inputs[i].onclick = function () { setMDNS(this.checked) };
  }
}

function setMDNS(v) {
  ajaxSpin("POST", "/services/update?mdns_enable=" + (v ? 1 : 0), function () {
    showNotification("mDNS is now " + (v ? "enabled" : "disabled"));
  }, function () {
    showWarning("Enable/disable failed");
    window.setTimeout(fetchServices, 100);
  });
}

function fetchServices() {
  ajaxJson("GET", "/services/info", displayServices, function () {
    window.setTimeout(fetchServices, 1000);
  });
}
