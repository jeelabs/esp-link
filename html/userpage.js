//===== Java script for user pages

var loadCounter = 0;
var refreshRate = 0;
var refreshTimer;

function notifyResponse( data )
{
  Object.keys(data).forEach(function(v) {
    var elems = document.getElementsByName(v);
    var ndx;
    for(ndx = 0; ndx < elems.length; ndx++ )
    {
      var el = elems[ndx];
      if(el.tagName == "INPUT")
      {
        if( el.type == "radio" )
        {
          el.checked = data[v] == el.value;
        }
        else
        {
          el.value = data[v];
        }
      }
      if(el.tagName == "SELECT")
      {
        el.value = data[v];
      }
    }
    var elem = document.getElementById(v);
    if( elem != null )
    {
      if(elem.tagName == "P" || elem.tagName == "DIV" || elem.tagName == "SPAN" || elem.tagName == "TR" || elem.tagName == "TH" || elem.tagName == "TD" ||
         elem.tagName == "TEXTAREA" )
      {
        elem.innerHTML = data[v];
      }
      if(elem.tagName == "UL" || elem.tagName == "OL")
      {
        var list = data[v];
        var html = "";

        for (var i=0; i<list.length; i++) {
          html = html.concat("<li>" + list[i] + "</li>");
        }

        elem.innerHTML = html;
      }
      if(elem.tagName == "TABLE")
      {
        var list = data[v];
        var html = "";

        if( list.length > 0 )
        {
          var ths = list[0];
          html = html.concat("<tr>");

          for (var i=0; i<ths.length; i++) {
            html = html.concat("<th>" + ths[i] + "</th>");
          }

          html = html.concat("</tr>");
        }

        for (var i=1; i<list.length; i++) {
          var tds = list[i];
          html = html.concat("<tr>");

          for (var j=0; j<tds.length; j++) {
            html = html.concat("<td>" + tds[j] + "</td>");
          }
          
          html = html.concat("</tr>");
        }
        
        elem.innerHTML = html;
      }
    }
  });
  
  if( refreshRate != 0 )
  {
    clearTimeout(refreshTimer);
    refreshTimer = setTimeout( function () {
      ajaxJson("GET", window.location.pathname + ".json?reason=refresh", notifyResponse );
    }, refreshRate );
  }
}

function notifyButtonPressed( btnId )
{
  ajaxJson("POST", window.location.pathname + ".json?reason=button\&id=" + btnId, notifyResponse);
}

function refreshFormData()
{
  setTimeout( function () {
    ajaxJson("GET", window.location.pathname + ".json?reason=refresh", function (resp) {
      notifyResponse(resp);
      if( loadCounter > 0 )
      {
        loadCounter--;
        refreshFormData();
      }
    } );
  } , 250);
}

document.addEventListener("DOMContentLoaded", function(){
  // collect buttons
  var btns = document.getElementsByTagName("button");
  var ndx;

  for (ndx = 0; ndx < btns.length; ndx++) {
    var btn = btns[ndx];
    var id = btn.getAttribute("id");
    var onclk = btn.getAttribute("onclick");
    var type = btn.getAttribute("type");
    
    if( id != null && onclk == null && type == "button" )
    {
      var fn;
      eval( "fn = function() { notifyButtonPressed(\"" + id + "\") }" );
      btn.onclick = fn;
    }
  }

  // collect forms
  var frms = document.getElementsByTagName("form");

  for (ndx = 0; ndx < frms.length; ndx++) {
    var frm = frms[ndx];
    
    var method = frm.method;
    var action = frm.action;
    
    frm.method = "POST";
    frm.action = window.location.pathname + ".json?reason=submit";
    loadCounter = 4;

    frm.onsubmit = function () {
      refreshFormData();
      return true;
    };
  }

  // collect metas
  var metas = document.getElementsByTagName("meta");

  for (ndx = 0; ndx < metas.length; ndx++) {
    var meta = metas[ndx];
    if( meta.getAttribute("name") == "refresh-rate" )
    {
      refreshRate = meta.getAttribute("content");
    }
  }

  // load variables at first time
  var loadVariables = function() {
    ajaxJson("GET", window.location.pathname + ".json?reason=load", notifyResponse,
      function () { setTimeout(loadVariables, 1000); }
    );
  };
  loadVariables();
});
