function flashFirmware(b){b.preventDefault();
var a=document.getElementById("fw-file").files[0];
$("#fw-form").setAttribute("hidden","");
$("#fw-spinner").removeAttribute("hidden");
showNotification("Firmware is being updated ...");
ajaxReq("POST","/flash/upload",function(c){ajaxReq("GET","/flash/reboot",function(d){showNotification("Firmware has been successfully updated!");
setTimeout(function(){window.location.reload()
},4000);
$("#fw-spinner").setAttribute("hidden","");
$("#fw-form").removeAttribute("hidden")
})
},null,a)
}function fetchFlash(){ajaxReq("GET","/flash/next",function(a){$("#fw-slot").innerHTML=a;
$("#fw-spinner").setAttribute("hidden","");
$("#fw-form").removeAttribute("hidden")
});
ajaxJson("GET","/menu",function(b){var a=$("#current-fw");
if(a!=null){a.innerHTML=b.version
}})
};