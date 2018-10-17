var specials=[];
specials.ap_ssid="SSID name";
specials.ap_password="PASSWORD";
specials.ap_maxconn="Max Connections number";
specials.ap_beacon="Beacon Interval";
function changeWifiMode(a){blockScan=1;
hideWarning();
ajaxSpin("POST","setmode?mode="+a,function(b){showNotification("Mode changed");
window.setTimeout(getWifiInfo,100);
blockScan=0
},function(c,b){showWarning("Error changing mode: "+b);
window.setTimeout(getWifiInfo,100);
blockScan=0
})
}function changeApSettings(j){j.preventDefault();
var a="/wifi/apchange?100=1";
var f,h=document.querySelectorAll("#"+j.target.id+" input,select");
for(f=0;
f<h.length;
f++){if(h[f].type=="checkbox"){var c=(h[f].checked)?1:0;
a+="&"+h[f].name+"="+c
}else{var k=h[f].value.replace(/[^!-~]/g,"");
var g=k.localeCompare(h[f].value);
if(g!=0){showWarning("Invalid characters in "+specials[h[f].name]);
return
}a+="&"+h[f].name+"="+k
}}hideWarning();
var b=j.target.id.replace("-form","");
var d=$("#"+b+"-button");
addClass(d,"pure-button-disabled");
ajaxSpin("POST",a,function(e){showNotification(b+" updated");
removeClass(d,"pure-button-disabled");
window.setTimeout(getWifiInfo,100)
},function(i,e){showWarning(e);
removeClass(d,"pure-button-disabled");
window.setTimeout(fetchApSettings,2000)
})
}function displayApSettings(a){Object.keys(a).forEach(function(b){el=$("#"+b);
if(el!=null){if(el.nodeName==="INPUT"){el.value=a[b]
}else{el.innerHTML=a[b]
}return
}el=document.querySelector('input[name="'+b+'"]');
if(el==null){el=document.querySelector('select[name="'+b+'"]')
}if(el!=null){if(el.type=="checkbox"){el.checked=a[b]=="enabled"
}else{el.value=a[b]
}}});
$("#AP_Settings-spinner").setAttribute("hidden","");
$("#AP_Settings-form").removeAttribute("hidden");
showWarning("Don't modify SOFTAP parameters with active connections");
window.setTimeout(hideWarning(),2000)
}function fetchApSettings(){ajaxJson("GET","/wifi/apinfo",displayApSettings,function(){window.setTimeout(fetchApSettings,1000)
})
};