//===== Java script for user pages


function notifyResponse( data )
{
  Object.keys(data).forEach(function(v) {
    var elem = document.getElementById(v);
    if( elem != null )
    {
      if(elem.tagName == "P" || elem.tagName == "DIV")
      {
	elem.innerHTML = data[v];
      }
    }
  });
}

function notifyButtonPressed( btnId )
{
  ajaxJson("POST", window.location.pathname + ".json?reason=button\&id=" + btnId, notifyResponse);
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
  
  // load variables at first time
  var loadVariables = function() {
    ajaxJson("GET", window.location.pathname + ".json?reason=load", notifyResponse,
      function () { setTimeout(loadVariables, 1000); }
    );
  };
  loadVariables();
});
