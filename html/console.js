function fetchText(delay) {
  el = $("#console");
  if (el.textEnd == undefined) {
    el.textEnd = 0;
    el.innerHTML = "";
  }
  window.setTimeout(function() {
    ajaxJson('GET', console_url + "?start=" + el.textEnd, updateText, retryLoad);
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
  fetchText(delay);
}

function retryLoad() {
  fetchText(1000);
}
