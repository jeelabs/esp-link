function fetchText(delay, repeat) {
  el = $("#console");
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
  el = $("#console");

  delay = 3000;
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
