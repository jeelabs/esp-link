function fetchText(delay, repeat) {
  var el = $("#console");
  if (el.textEnd == undefined) {
    el.textEnd = 0;
    el.innerHTML = "";
  }
  window.setTimeout(function() {
    ajaxJson('GET', console_url + "?start=" + el.textEnd,
      function(resp) {
        var dly = updateText(resp);
        if (repeat) fetchText(dly, repeat);
      },
      function() { retryLoad(repeat); });
  }, delay);
}

function updateText(resp) {
  var el = $("#console");

  var delay = 3000;
  if (resp != null && resp.len > 0) {
    console.log("updateText got", resp.len, "chars at", resp.start);
    if (resp.start > el.textEnd) {
      el.innerHTML = el.innerHTML.concat("\r\n<missing lines\r\n");
    }
    el.innerHTML = el.innerHTML.concat(resp.text);
    el.textEnd = resp.start + resp.len;
    delay = 500;
  }
  return delay;
}

function retryLoad(repeat) {
  fetchText(1000, repeat);
}

//===== Console page

function showRate(rate) {
  rates.forEach(function(r) {
    var el = $("#"+r+"-button");
    el.className = el.className.replace(" button-selected", "");
  });

  var el = $("#"+rate+"-button");
  if (el != null) el.className += " button-selected";
}

function baudButton(baud) {
  $("#baud-btns").appendChild(m(
    ' <a id="'+baud+'-button" href="#" class="pure-button">'+baud+'</a>'));

  $("#"+baud+"-button").addEventListener("click", function(e) {
    e.preventDefault();
    ajaxSpin('POST', "/console/baud?rate="+baud,
      function(resp) { showNotification("" + baud + " baud set"); showRate(baud); },
      function(s, st) { showWarning("Error setting baud rate: " + st); }
    );
  });
}

//===== Log page

function showDbgMode(mode) {
  var btns = $('.dbg-btn');
  for (var i=0; i < btns.length; i++) {
    if (btns[i].id === "dbg-"+mode)
      addClass(btns[i], "button-selected");
    else
      removeClass(btns[i], "button-selected");
  }
}
