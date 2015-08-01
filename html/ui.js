//===== Collection of small utilities

/*
 * Bind/Unbind events
 *
 * Usage:
 *   var el = document.getElementyById('#container');
 *   bnd(el, 'click', function() {
 *     console.log('clicked');
 *   });
 */

var bnd = function(
  d, // a DOM element
  e, // an event name such as "click"
  f  // a handler function
){
  d.addEventListener(e, f, false);
}

/*
 * Create DOM element
 *
 * Usage:
 *   var el = m('<h1>Hello</h1>');
 *   document.body.appendChild(el);
 *
 * Copyright (C) 2011 Jed Schmidt <http://jed.is> - WTFPL
 * More: https://gist.github.com/966233
 */

var m = function(
  a, // an HTML string
  b, // placeholder
  c  // placeholder
){
  b = document;                   // get the document,
  c = b.createElement("p");       // create a container element,
  c.innerHTML = a;                // write the HTML to it, and
  a = b.createDocumentFragment(); // create a fragment.

  while (                         // while
    b = c.firstChild              // the container element has a first child
  ) a.appendChild(b);             // append the child to the fragment,

  return a                        // and then return the fragment.
}

/*
 * DOM selector
 *
 * Usage:
 *   $('div');
 *   $('#name');
 *   $('.name');
 *
 * Copyright (C) 2011 Jed Schmidt <http://jed.is> - WTFPL
 * More: https://gist.github.com/991057
 */

var $ = function(
  a,                         // take a simple selector like "name", "#name", or ".name", and
  b                          // an optional context, and
){
  a = a.match(/^(\W)?(.*)/); // split the selector into name and symbol.
  return(                    // return an element or list, from within the scope of
    b                        // the passed context
    || document              // or document,
  )[
    "getElement" + (         // obtained by the appropriate method calculated by
      a[1]
        ? a[1] == "#"
          ? "ById"           // the node by ID,
          : "sByClassName"   // the nodes by class name, or
        : "sByTagName"       // the nodes by tag name,
    )
  ](
    a[2]                     // called with the name.
  )
}

/*
 * Get cross browser xhr object
 *
 * Copyright (C) 2011 Jed Schmidt <http://jed.is>
 * More: https://gist.github.com/993585
 */

var j = function(
  a // cursor placeholder
){
  for(                     // for all a
    a=0;                   // from 0
    a<4;                   // to 4,
    a++                    // incrementing
  ) try {                  // try
    return a               // returning
      ? new ActiveXObject( // a new ActiveXObject
          [                // reflecting
            ,              // (elided)
            "Msxml2",      // the various
            "Msxml3",      // working
            "Microsoft"    // options
          ][a] +           // for Microsoft implementations, and
          ".XMLHTTP"       // the appropriate suffix,
        )                  // but make sure to
      : new XMLHttpRequest // try the w3c standard first, and
  }

  catch(e){}               // ignore when it fails.
}

// createElement short-hand

e = function(a) { return document.createElement(a); }

// chain onload handlers

function onLoad(f) {
  var old = window.onload;
  if (typeof old != 'function') {
    window.onload = f;
  } else {
    window.onload = function() {
      old();
      f();
    }
  }
}

//===== helpers to add/remove/toggle HTML element classes

function addClass(el, cl) {
  el.className += ' ' + cl;
}
function removeClass(el, cl) {
  var cls = el.className.split(/\s+/),
      l = cls.length;
  for (var i=0; i<l; i++) {
    if (cls[i] === cl) cls.splice(i, 1);
  }
  el.className = cls.join(' ');
  return cls.length != l
}
function toggleClass(el, cl) {
  if (!removeClass(el, cl)) addClass(el, cl);
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

//===== main menu, header spinner and notification boxes

onLoad(function() {
  var l = $("#layout");
  var o = l.childNodes[0];
  // spinner
  l.insertBefore(m('<div id="spinner" class="spinner" hidden></div>'), o);
  // notification boxes
  l.insertBefore(m(
    '<div id="messages"><div id="warning" hidden></div><div id="notification" hidden></div></div>'), o);
  // menu hamburger button
  l.insertBefore(m('<a href="#menu" id="menuLink" class="menu-link"><span></span></a>'), o);
  // menu left-pane
  var mm = m(
   '<div id="menu">\
      <div class="pure-menu">\
        <a class="pure-menu-heading" href="https://github.com/jeelabs/esp-link">\
        <img src="/favicon.ico" height="32">&nbsp;esp-link</a>\
        <ul id="menu-list" class="pure-menu-list"></ul>\
      </div>\
    </div>\
    ');
  l.insertBefore(mm, o);

  // make hamburger button pull out menu
  var ml = $('#menuLink'), mm = $('#menu');
  bnd(ml, 'click', function (e) {
    console.log("hamburger time");
      var active = 'active';
      e.preventDefault();
      toggleClass(l, active);
      toggleClass(mm, active);
      toggleClass(ml, active);
  });

  // populate menu via ajax call
  var getMenu = function() {
    ajaxJson("GET", "/menu", function(data) {
      var html = "", path = window.location.pathname;
      for (var i=0; i<data.menu.length; i+=2) {
        var href = data.menu[i+1];
        html = html.concat(" <li class=\"pure-menu-item" +
            (path === href ? " pure-menu-selected" : "") + "\">" +
            "<a href=\"" + href + "\" class=\"pure-menu-link\">" +
            data.menu[i] + "</a></li>");
      }
      $("#menu-list").innerHTML = html;

      v = $("#version");
      if (v != null) { v.innerHTML = data.version; }
    }, function() { setTimeout(getMenu, 1000); });
  };
  getMenu();
});

//===== Wifi info

function showWifiInfo(data) {
  Object.keys(data).forEach(function(v) {
    el = $("#wifi-" + v);
    if (el != null) {
      if (el.nodeName === "INPUT") el.value = data[v];
      else el.innerHTML = data[v];
    }
  });
  var dhcp = $('#dhcp-r'+data.dhcp);
  if (dhcp) dhcp.click();
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

//===== GPIO Pin mux card

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

//===== TCP client card

function tcpEn(){return document.querySelector('input[name="tcp_enable"]')}
function rssiEn(){return document.querySelector('input[name="rssi_enable"]')}
function apiKey(){return document.querySelector('input[name="api_key"]')}

function changeTcpClient(e) {
  e.preventDefault();
  var url = "tcpclient";
  url += "?tcp_enable=" + tcpEn().checked;
  url += "&rssi_enable=" + rssiEn().checked;
  url += "&api_key=" + encodeURIComponent(apiKey().value);

  hideWarning();
  var cb = $("#tcp-button");
  addClass(cb, 'pure-button-disabled');
  ajaxSpin("POST", url, function(resp) {
      removeClass(cb, 'pure-button-disabled');
      getWifiInfo();
    }, function(s, st) {
      showWarning("Error: "+st);
      removeClass(cb, 'pure-button-disabled');
      getWifiInfo();
    });
}

function displayTcpClient(resp) {
  tcpEn().checked = resp.tcp_enable > 0;
  rssiEn().checked = resp.rssi_enable > 0;
  apiKey().value = resp.api_key;
}

function fetchTcpClient() {
  ajaxJson("GET", "/tcpclient", displayTcpClient, function() {
		window.setTimeout(fetchTcpClient, 1000);
	});
}


