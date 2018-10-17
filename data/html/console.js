//===== Fetching console text

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
//    console.log("updateText got", resp.len, "chars at", resp.start);
    var isScrolledToBottom = el.scrollHeight - el.clientHeight <= el.scrollTop + 1;
    //console.log("isScrolledToBottom="+isScrolledToBottom, "scrollHeight="+el.scrollHeight,
    //            "clientHeight="+el.clientHeight, "scrollTop="+el.scrollTop,
    //            "" + (el.scrollHeight - el.clientHeight) + "<=" + (el.scrollTop + 1));

    // append the text
    if (resp.start > el.textEnd) {
      el.innerHTML = el.innerHTML.concat("\r\n<missing lines\r\n");
    }
    el.innerHTML = el.innerHTML.concat(resp.text
       .replace(/&/g, '&amp;')
       .replace(/</g, '&lt;')
       .replace(/>/g, '&gt;')
       .replace(/"/g, '&quot;'));
    el.textEnd = resp.start + resp.len;
    delay = 500;

    // scroll to bottom
    if(isScrolledToBottom) el.scrollTop = el.scrollHeight - el.clientHeight;
  }
  return delay;
}

function retryLoad(repeat) {
  fetchText(1000, repeat);
}

//===== Text entry

function consoleSendInit() {
  var sendHistory = $("#send-history");
  var inputText = $("#input-text");
  var inputAddCr = $("#input-add-cr");
  var inputAddLf = $("#input-add-lf");

  function findHistory(text) {
    for (var i = 0; i < sendHistory.children.length; i++) {
      if (text == sendHistory.children[i].value) {
        return i;
      }
    }
    return null;
  }

  function loadHistory(idx) {
    sendHistory.value = sendHistory.children[idx].value;
    inputText.value = sendHistory.children[idx].value;
  }

  function navHistory(rel) {
    var idx = findHistory(sendHistory.value) + rel;
    if (idx < 0) {
      idx = sendHistory.children.length - 1;
    }
    if (idx >= sendHistory.children.length) {
      idx = 0;
    }
    loadHistory(idx);
  }

  sendHistory.addEventListener("change", function(e) {
    inputText.value = sendHistory.value;
  });

  function pushHistory(text) {
    var idx = findHistory(text);
    if (idx !== null) {
      loadHistory(idx);
      return false;
    }
    var newOption = m('<option>'+
      (text
       .replace(/&/g, '&amp;')
       .replace(/</g, '&lt;')
       .replace(/>/g, '&gt;')
       .replace(/"/g, '&quot;'))
                     +'</option>');
    newOption.value = text;
    sendHistory.appendChild(newOption);
    sendHistory.value = text;
    for (; sendHistory.children.length > 15; ) {
      sendHistory.removeChild(sendHistory.children[0]);
    }
    return true;
  }

  inputText.addEventListener("keydown", function(e) {
    switch (e.keyCode) {
      case 38: /* the up arrow key pressed */
        e.preventDefault();
        navHistory(-1);
        break;
      case 40: /* the down arrow key pressed */
        e.preventDefault();
        navHistory(+1);
        break;
      case 27: /* the escape key pressed */
        e.preventDefault();
        inputText.value = "";
        sendHistory.value = "";
        break;
      case 13: /* the enter key pressed */
        e.preventDefault();
        var text = inputText.value;
        if (inputAddCr.checked) text += '\r';
        if (inputAddLf.checked) text += '\n';
        pushHistory(inputText.value);
        inputText.value = "";
        ajaxSpin('POST', "/console/send?text=" + encodeURIComponent(text),
          function(resp) { showNotification("Text sent"); },
          function(s, st) { showWarning("Error sending text"); }
        );
        break;
    }
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
