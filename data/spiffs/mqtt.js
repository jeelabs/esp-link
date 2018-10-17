function changeMqtt(f){f.preventDefault();
var c="mqtt?1=1";
var d,b=document.querySelectorAll("#mqtt-form input");
for(d=0;
d<b.length;
d++){if(b[d].type!="checkbox"){c+="&"+b[d].name+"="+b[d].value
}}hideWarning();
var a=$("#mqtt-button");
addClass(a,"pure-button-disabled");
ajaxSpin("POST",c,function(e){showNotification("MQTT updated");
removeClass(a,"pure-button-disabled")
},function(g,e){showWarning("Error: "+e);
removeClass(a,"pure-button-disabled");
window.setTimeout(fetchMqtt,100)
})
}function displayMqtt(c){Object.keys(c).forEach(function(d){el=$("#"+d);
if(el!=null){if(el.nodeName==="INPUT"){el.value=c[d]
}else{el.innerHTML=c[d]
}return
}el=document.querySelector('input[name="'+d+'"]');
if(el!=null){if(el.type=="checkbox"){el.checked=c[d]>0
}else{el.value=c[d]
}}});
$("#mqtt-spinner").setAttribute("hidden","");
$("#mqtt-status-spinner").setAttribute("hidden","");
$("#mqtt-form").removeAttribute("hidden");
$("#mqtt-status-form").removeAttribute("hidden");
var b,a=$("input");
for(b=0;
b<a.length;
b++){if(a[b].type=="checkbox"){a[b].onclick=function(){setMqtt(this.name,this.checked)
}
}}}function fetchMqtt(){ajaxJson("GET","/mqtt",displayMqtt,function(){window.setTimeout(fetchMqtt,1000)
})
}function changeMqttStatus(b){b.preventDefault();
var a=document.querySelector('input[name="mqtt-status-topic"]').value;
ajaxSpin("POST","/mqtt?mqtt-status-topic="+a,function(){showNotification("MQTT status settings updated")
},function(d,c){showWarning("Error: "+c);
window.setTimeout(fetchMqtt,100)
})
}function setMqtt(b,a){ajaxSpin("POST","/mqtt?"+b+"="+(a?1:0),function(){var c=b.replace("-enable","");
showNotification(c+" is now "+(a?"enabled":"disabled"))
},function(){showWarning("Enable/disable failed");
window.setTimeout(fetchMqtt,100)
})
};