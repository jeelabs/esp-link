//===== FLASH cards

function flashFirmware(e) {
  e.preventDefault();
	var fw_data = document.getElementById('fw-file').files[0];
			
	$("#fw-form").setAttribute("hidden", "");
	$("#fw-spinner").removeAttribute("hidden");
  showNotification("Firmware is being updated ...");

	nanoajax.ajax({url: '/flash/upload', method: 'POST', body: fw_data}, function (code, responseText, request) {
		if(""+code == "200")
		{
			ajaxReq("GET", "/flash/reboot", function (resp) {
    		showNotification("Firmware has been successfully updated!");
				setTimeout(function(){ window.location.reload()}, 4000);

				$("#fw-spinner").setAttribute("hidden", "");
				$("#fw-form").removeAttribute("hidden");
			});
		}
	})
}

function fetchFlash() {
  ajaxReq("GET", "/flash/next", function (resp) {
		$("#fw-slot").innerHTML = resp;
  	$("#fw-spinner").setAttribute("hidden", "");
  	$("#fw-form").removeAttribute("hidden");
  });
	ajaxJson("GET", "/menu", function(data) {
      var v = $("#current-fw");
      if (v != null) { v.innerHTML = data.version; }
    }
	);
}

function setMqtt(name, v) {
  
}
