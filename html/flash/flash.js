onLoad(function() {
  bnd($("#auth-form"), "submit", changeAuth);  
  bnd($("#flash-start"), "click", startUpload);  
  bnd($("#flash-stop"), "click", stopUpload);  
  nextFlash();
  fetchAuth();
});

// disable START button, enable STOP button, show spinner
function flash_start()
{
  var cb = $("#flash-start");
  addClass(cb, "pure-button-disabled");
  cb.setAttribute("disabled", "");
  var cb = $("#flash-stop");
  removeClass(cb, "pure-button-disabled");
  cb.removeAttribute("disabled");
  $("#firmware-spinner").removeAttribute("hidden");
  $("#progressbar").value = 0;
  $("#up-count").innerHTML = '0 %';
  $("#progress").removeAttribute("hidden");   
}

// enable START button, disable STOP button, hide spinner
function flash_stop()
{
  var cb = $("#flash-start");
  removeClass(cb, "pure-button-disabled");
  cb.removeAttribute("disabled");
  var cb = $("#flash-stop");
  addClass(cb, "pure-button-disabled");
  cb.setAttribute("disabled", "true");
  $("#firmware-spinner").setAttribute("hidden","true");
  $("#progress").setAttribute("hidden","true");   
} 

function displayAuth(data) {
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

  $("#auth-spinner").setAttribute("hidden", "");
  $("#auth-form").removeAttribute("hidden");
} 

function fetchAuth() {
  ajaxJson("GET", "/flash/auth", displayAuth, function () {
    window.setTimeout(fetchAuth, 1000);
  });
}

function showFlash(data) {
  el = $("#firmware-name");
  if (el != null) el.innerHTML = data;

  $("#firmware-spinner").setAttribute("hidden", "");
  $("#firmware-form").removeAttribute("hidden");
}

function nextFlash() {
  ajaxReq("GET", "/flash/next", showFlash, function () {
    window.setTimeout(nextFlash, 1000);
  });
}

function showFlash2(data) {
  el = $("#firmware-name");
  if (el != null) 
  {
    if(el.innerHTML != data) showNotification("Firmware successfully updated");
      else showWarning("Flashing seems to have failed and it reverted to the old firmware?");
    el.innerHTML = data;
  }
  // show uploading form
  $("#firmware-spinner").setAttribute("hidden", "");
  $("#firmware-form").removeAttribute("hidden");
}

// waiting module to come up after reboot
function checkFlash() {
  ajaxReq("GET", "/flash/next", showFlash2, function () {
    window.setTimeout(checkFlash, 1000);
  });
}

function nextReboot() {
  // hide uploading form
  $("#firmware-spinner").removeAttribute("hidden");
  $("#firmware-form").setAttribute("hidden","");
  ajaxReq("GET", "/flash/reboot", 
    function(data)
    {
      showNotification("Waiting for ESP module to reboot");
      window.setTimeout(checkFlash, 4000); 
    },
    function (s,st) 
    {
      showWarning("ERROR - "+st);
      // show uploading form
      $("#firmware-spinner").setAttribute("hidden", "");
      $("#firmware-form").removeAttribute("hidden");
    }
  );
}

function changeAuth(e) {
  e.preventDefault();
  var url = "/flash/auth?1=1";
  var i, inputs = document.querySelectorAll("#" + e.target.id + " input,select");
  for (i = 0; i < inputs.length; i++) 
  {
    if (inputs[i].type == "checkbox") 
    {
      var val = (inputs[i].checked) ? 1 : 0;
      url += "&" + inputs[i].name + "=" + val;
    }
    else url += "&" + inputs[i].name + "=" + inputs[i].value;
  };

  hideWarning();
  var n = e.target.id.replace("-form", "");
  var cb = $("#" + n + "-button");
  addClass(cb, "pure-button-disabled");
  ajaxSpin("POST", url, function (resp) {
    showNotification("Settings updated");
    removeClass(cb, "pure-button-disabled");
  }, function (s, st) {
    showWarning("Error: " + st);
    removeClass(cb, "pure-button-disabled");
    window.setTimeout(fetchAuth, 100);
  });
} 

function errUpload(evt)
{
  flash_stop();
}

function startUpload()
{
  hideWarning();
  var file_input = $("#firmware_file");
  if(file_input.files.length==0)
  {
    alert("Please choose a file");
    return;
  }
  var progressBar = $("#progressbar");
  flash_start();
  var xhr = j();
  xhr.upload.onabort = errUpload;
  xhr.upload.onerror = errUpload;
  xhr.upload.ontimeout = errUpload;
  xhr.upload.onprogress =  function (e) {
    if (e.lengthComputable) 
    {
      progressBar.max = e.total;
      progressBar.value = e.loaded;
      $("#up-count").innerHTML = Math.floor((e.loaded / e.total) * 100) + '%';
    }
  }
  xhr.upload.onloadstart = function (e) {
    progressBar.value = 0;
  }
  xhr.upload.onloadend = function (e) {
    progressBar.value = e.loaded;
  }
  xhr.onreadystatechange = function() { 
    if (xhr.readyState != 4) return;
    clearTimeout(timeout);
    if (xhr.status >= 200 && xhr.status < 300) 
    {
      //console.log("XHR done:", method, url, "->", xhr.status);
      flash_stop();
      showNotification("Firmware updated - now rebooting");
      window.setTimeout(nextReboot, 3000);
    } 
    else 
    {
      console.log("XHR ERR : POST /flash/upload -> ", xhr.status, xhr.responseText, xhr);
      flash_stop();
      showWarning("ERROR = " + xhr.responseText);
    }
  }
  xhr.open("POST", "/flash/upload", true);
  xhr.setRequestHeader('Content-Type', "application/octet-stream");
  xhr.setRequestHeader('Content-Disposition', 'attachment; filename="' + $("#firmware-name").innerHTML + '"');
  var timeout = setTimeout(function() {
    xhr.abort();
    console.log("XHR abort: POST /flash/upload");
    xhr.status = 599;
    xhr.responseText = "request time-out";
  }, 9000);
  //console.log("XHR send:", method, url);
  try 
  {
    xhr.send(file_input.files[0]);
  } 
  catch(err) 
  {
    console.log("XHR EXEC : POST /flash/upload -> ", err);
    flash_stop();
    showWarning("Error = " + err);
  }   
}

function stopUpload()
{
	if(req) req.abort();
	req = null; 
	flash_stop();
}