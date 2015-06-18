// fill out menu items
(function() {
    html = "";
    for (var i=0; i<menu.length; i+=2) {
      html = html.concat(" <li class=\"pure-menu-item\"><a href=\"" + menu[i+1] +
          "\" class=\"pure-menu-link\">" + menu[i] + "</a></li>");
    }
    document.getElementById("menu-list").innerHTML = html;
    v = document.getElementById("version");
    if (v != null) { v.innerHTML = version; }
}());

(function (window, document) {

    var layout   = document.getElementById('layout'),
        menu     = document.getElementById('menu'),
        menuLink = document.getElementById('menuLink');

    function toggleClass(element, className) {
        var classes = element.className.split(/\s+/),
            length = classes.length,
            i = 0;

        for(; i < length; i++) {
          if (classes[i] === className) {
            classes.splice(i, 1);
            break;
          }
        }
        // The className is not found
        if (length === classes.length) {
            classes.push(className);
        }

        element.className = classes.join(' ');
    }

    menuLink.onclick = function (e) {
        var active = 'active';

        e.preventDefault();
        toggleClass(layout, active);
        toggleClass(menu, active);
        toggleClass(menuLink, active);
    };

}(this, this.document));

//===== Wifi info

function showWifiInfo(data) {
  Object.keys(data).forEach(function(v) {
    el = $("#wifi-" + v);
    if (el != null) el.innerHTML = data[v];
  });
  $("#wifi-spinner").setAttribute("hidden", "");
  $("#wifi-table").removeAttribute("hidden");
  currAp = data.ssid;
}

function getWifiInfo() {
  ajaxJson('GET', "/wifi/info", showWifiInfo,
      function(s, st) { window.setTimeout(getWifiInfo, 1000); });
}

//===== Notifications

function showWarning(text) {
  var el = $("#warning");
  el.innerHTML = text;
  el.removeAttribute('hidden');
}
function hideWarning() {
  el = $("#warning").setAttribute('hidden', '');
}
var notifTimeout = null;
function showNotification(text) {
  var el = $("#notification");
  el.innerHTML = text;
  el.removeAttribute('hidden');
  if (notifTimeout != null) clearTimeout(notifTimeout);
  notifTimout = setTimeout(function() {
      el.setAttribute('hidden', '');
      notifTimout = null;
    }, 4000);
}

//===== AJAX

function ajaxReq(method, url, ok_cb, err_cb) {
  var xhr = j();
  xhr.open(method, url, true);
  var timeout = setTimeout(function() {
    xhr.abort();
    console.log("XHR abort:", method, url);
    xhr.status = 599;
    xhr.responseText = "request time-out";
  }, 9000);
  xhr.onreadystatechange = function() {
    if (xhr.readyState != 4) { return; }
    clearTimeout(timeout);
    if (xhr.status >= 200 && xhr.status < 300) {
      console.log("XHR done:", method, url, "->", xhr.status);
      ok_cb(xhr.responseText);
    } else {
      console.log("XHR ERR :", method, url, "->", xhr.status, xhr.responseText, xhr);
      err_cb(xhr.status, xhr.responseText);
    }
  }
  console.log("XHR send:", method, url);
  try {
    xhr.send();
  } catch(err) {
    console.log("XHR EXC :", method, url, "->", err);
    err_cb(599, err);
  }
}

function dispatchJson(resp, ok_cb, err_cb) {
  var j;
  try { j = JSON.parse(resp); }
  catch(err) {
    console.log("JSON parse error: " + err + ". In: " + resp);
    err_cb(500, "JSON parse error: " + err);
    return;
  }
  ok_cb(j);
}

function ajaxJson(method, url, ok_cb, err_cb) {
  ajaxReq(method, url, function(resp) { dispatchJson(resp, ok_cb, err_cb); }, err_cb);
}

function ajaxSpin(method, url, ok_cb, err_cb) {
  $("#spinner").removeAttribute('hidden');
  ajaxReq(method, url, function(resp) {
      $("#spinner").setAttribute('hidden', '');
      ok_cb(resp);
    }, function(status, statusText) {
      $("#spinner").setAttribute('hidden', '');
      //showWarning("Error: " + statusText);
      err_cb(status, statusText);
    });
}

function ajaxJsonSpin(method, url, ok_cb, err_cb) {
  ajaxSpin(method, url, function(resp) { dispatchJson(resp, ok_cb, err_cb); }, err_cb);
}


