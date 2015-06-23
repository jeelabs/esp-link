  <div id="main">
    <div class="header">
      <h1>Microcontroller Console</h1>
    </div>

    <div class="content">
      <p>The Microcontroller console shows the last 1024 characters
         received from UART0, to which a microcontroller is typically attached.
         The UART is configured for 8 bits, no parity, 1 stop bit (8N1).</p>
      <p>
        <a id="reset-button" class="pure-button button-primary" href="#">Reset µC</a>
        &nbsp;Baud:
        <span id="baud-btns"></span>
      </p>
      <pre class="console" id="console"></pre>
    </div>
  </div>
</div>

<script src="ui.js"></script>
<script type="text/javascript">console_url = "/console/text"</script>
<script src="console.js"></script>
<script type="text/javascript">
  var rates = [57600, 115200, 230400, 460800];

  function showRate(rate) {
      rates.forEach(function(r) {
        var el = $("#"+r+"-button");
        el.className = el.className.replace(" button-selected", "");
      });

      var el = $("#"+rate+"-button");
      if (el != null) el.className += " button-selected";
    }

  function baudButton(baud) {
    $("#baud-btns").appendChild(m(
      ' <a id="'+baud+'-button" href="#" class="pure-button">'+baud+'</a>'));

    $("#"+baud+"-button").addEventListener("click", function(e) {
      e.preventDefault();
      ajaxSpin('POST', "/console/baud?rate="+baud,
        function(resp) { showNotification("" + baud + " baud set"); showRate(baud); },
        function(s, st) { showWarning("Error setting baud rate: ", st); }
      );
    });
  }

  window.onload = function() {
    fetchText(100, true);

    $("#reset-button").addEventListener("click", function(e) {
      e.preventDefault();
      $("#console").innerHTML = "";
      ajaxSpin('POST', "/console/reset",
        function(resp) { showNotification("uC reset"); },
        function(s, st) { showWarning("Error resetting uC"); }
      );
    });

    rates.forEach(function(r) { baudButton(r); });

    ajaxJson('GET', "/console/baud",
      function(data) { showRate(data.rate); },
      function(s, st) { showNotification(st); }
    );
  }
</script>
</body></html>
