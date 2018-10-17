var loadCounter=0;
var refreshRate=0;
var refreshTimer;
var hiddenInputs=[];
function notifyResponse(a){Object.keys(a).forEach(function(m){var c=document.getElementsByName(m);
var n;
for(n=0;
n<c.length;
n++){var d=c[n];
if(d.tagName=="INPUT"){if(d.type=="radio"){d.checked=a[m]==d.value
}else{if(d.type=="checkbox"){if(a[m]=="on"){d.checked=true
}else{if(a[m]=="off"){d.checked=false
}else{if(a[m]==true){d.checked=true
}else{d.checked=false
}}}}else{d.value=a[m]
}}}if(d.tagName=="SELECT"){d.value=a[m]
}}var e=document.getElementById(m);
if(e!=null){if(e.tagName=="P"||e.tagName=="DIV"||e.tagName=="SPAN"||e.tagName=="TR"||e.tagName=="TH"||e.tagName=="TD"||e.tagName=="TEXTAREA"){e.innerHTML=a[m]
}if(e.tagName=="UL"||e.tagName=="OL"){var l=a[m];
var k="";
for(var h=0;
h<l.length;
h++){k=k.concat("<li>"+l[h]+"</li>")
}e.innerHTML=k
}if(e.tagName=="TABLE"){var l=a[m];
var k="";
if(l.length>0){var b=l[0];
k=k.concat("<tr>");
for(var h=0;
h<b.length;
h++){k=k.concat("<th>"+b[h]+"</th>")
}k=k.concat("</tr>")
}for(var h=1;
h<l.length;
h++){var g=l[h];
k=k.concat("<tr>");
for(var f=0;
f<g.length;
f++){k=k.concat("<td>"+g[f]+"</td>")
}k=k.concat("</tr>")
}e.innerHTML=k
}}});
if(refreshRate!=0){clearTimeout(refreshTimer);
refreshTimer=setTimeout(function(){ajaxJson("GET",window.location.pathname+".json?reason=refresh",notifyResponse)
},refreshRate)
}}function notifyButtonPressed(a){ajaxJson("POST",window.location.pathname+".json?reason=button&id="+a,notifyResponse)
}function refreshFormData(){setTimeout(function(){ajaxJson("GET",window.location.pathname+".json?reason=refresh",function(a){notifyResponse(a);
if(loadCounter>0){loadCounter--;
refreshFormData()
}})
},250)
}function recalculateHiddenInputs(){for(var f=0;
f<hiddenInputs.length;
f++){var g=hiddenInputs[f];
var d=g.name;
var b=document.getElementsByName(d);
for(var c=0;
c<b.length;
c++){var a=b[c];
var e=a.type;
if(e=="checkbox"){if(a.checked){g.disabled=true;
g.value="on"
}else{g.disabled=false;
g.value="off"
}}}}}document.addEventListener("DOMContentLoaded",function(){var btns=document.getElementsByTagName("button");
var ndx;
for(ndx=0;
ndx<btns.length;
ndx++){var btn=btns[ndx];
var id=btn.getAttribute("id");
var onclk=btn.getAttribute("onclick");
var type=btn.getAttribute("type");
if(id!=null&&onclk==null&&type=="button"){var fn;
eval('fn = function() { notifyButtonPressed("'+id+'") }');
btn.onclick=fn
}}var frms=document.getElementsByTagName("form");
for(ndx=0;
ndx<frms.length;
ndx++){var frm=frms[ndx];
var method=frm.method;
var action=frm.action;
frm.method="POST";
frm.action=window.location.pathname+".json?reason=submit";
loadCounter=4;
frm.onsubmit=function(){recalculateHiddenInputs();
refreshFormData();
return true
}
}var metas=document.getElementsByTagName("meta");
for(ndx=0;
ndx<metas.length;
ndx++){var meta=metas[ndx];
if(meta.getAttribute("name")=="refresh-rate"){refreshRate=meta.getAttribute("content")
}}var inputs=document.getElementsByTagName("input");
for(ndx=0;
ndx<inputs.length;
ndx++){var inp=inputs[ndx];
if(inp.getAttribute("type")=="checkbox"){var name=inp.getAttribute("name");
var hasHidden=false;
if(name!=null){var inpelems=document.getElementsByName(name);
for(var i=0;
i<inpelems.length;
i++){var inptp=inpelems[i].type;
if(inptp=="hidden"){hasHidden=true
}}}if(!hasHidden){var parent=inp.parentElement;
var input=document.createElement("input");
input.type="hidden";
input.name=inp.name;
parent.appendChild(input);
hiddenInputs.push(input)
}}}var loadVariables=function(){ajaxJson("GET",window.location.pathname+".json?reason=load",notifyResponse,function(){setTimeout(loadVariables,1000)
})
};
loadVariables()
});