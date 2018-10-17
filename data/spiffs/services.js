function changeServices(f){f.preventDefault();
var c="services/update?1=1";
var d,b=document.querySelectorAll("#"+f.target.id+" input,select");
for(d=0;
d<b.length;
d++){if(b[d].type=="checkbox"){if(b[d].name.slice(-6)=="enable"){continue
}var g=(b[d].checked)?1:0;
c+="&"+b[d].name+"="+g
}else{c+="&"+b[d].name+"="+b[d].value
}}hideWarning();
var h=f.target.id.replace("-form","");
var a=$("#"+h+"-button");
addClass(a,"pure-button-disabled");
ajaxSpin("POST",c,function(e){showNotification(h+" updated");
removeClass(a,"pure-button-disabled")
},function(i,e){showWarning("Error: "+e);
removeClass(a,"pure-button-disabled");
window.setTimeout(fetchServices,100)
})
}function displayServices(c){Object.keys(c).forEach(function(d){el=$("#"+d);
if(el!=null){if(el.nodeName==="INPUT"){el.value=c[d]
}else{el.innerHTML=c[d]
}return
}el=document.querySelector('input[name="'+d+'"]');
if(el==null){el=document.querySelector('select[name="'+d+'"]')
}if(el!=null){if(el.type=="checkbox"){el.checked=c[d]=="enabled"
}else{el.value=c[d]
}}});
$("#syslog-spinner").setAttribute("hidden","");
$("#sntp-spinner").setAttribute("hidden","");
$("#mdns-spinner").setAttribute("hidden","");
if(c.syslog_host!==undefined){$("#Syslog-form").removeAttribute("hidden")
}else{$("#Syslog-form").parentNode.setAttribute("hidden","")
}$("#SNTP-form").removeAttribute("hidden");
$("#mDNS-form").removeAttribute("hidden");
var b,a=$("input");
for(b=0;
b<a.length;
b++){if(a[b].name=="mdns_enable"){a[b].onclick=function(){setMDNS(this.checked)
}
}}}function setMDNS(a){ajaxSpin("POST","/services/update?mdns_enable="+(a?1:0),function(){showNotification("mDNS is now "+(a?"enabled":"disabled"))
},function(){showWarning("Enable/disable failed");
window.setTimeout(fetchServices,100)
})
}function fetchServices(){ajaxJson("GET","/services/info",displayServices,function(){window.setTimeout(fetchServices,1000)
})
};