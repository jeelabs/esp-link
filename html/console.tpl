  <div id="main">
    <div class="header">
      <h1>Microcontroller Console</h1>
    </div>

    <div class="content">
      <p>The Microcontroller console shows the last 1024 characters
         received from UART0, to which a microcontroller is typically attached.</p>
      <p>
        <a id="reset-button" class="pure-button button-primary" href="#">Reset µC</a>
        &nbsp;Baud:
        <a id="57600-button" href="#" class="pure-button">57600</a>
        <a id="115200-button" href="#" class="pure-button">115200</a>
        <a id="230400-button" href="#" class="pure-button">230400</a>
        <a id="460800-button" href="#" class="pure-button">460800</a>
      </p>
      <pre class="console" id="console"></pre>
    </div>
  </div>
</div>

<script src="ui.js"></script>
<script type="text/javascript">console_url = "/console/text"</script>
<script src="console.js"></script>
<script type="text/javascript">
  function baudButton(baud) {
    $("#"+baud+"-button").addEventListener("click", function(e) {
      e.preventDefault();
      ajaxSpin('GET', "/console/baud?rate="+baud,
        function(resp) { showNotification("" + baud + " baud set"); },
        function(s, st) { showWarning("Error setting baud rate: ", st); }
      );
    });
  }

  window.onload = function() {
    fetchText(100, true);

    $("#reset-button").addEventListener("click", function(e) {
      e.preventDefault();
      $("#console").innerHTML = "";
      ajaxSpin('GET', "/console/reset",
        function(resp) { showNotification("uC reset"); },
        function(s, st) { showWarning("Error resetting uC"); }
      );
    });
    baudButton(57600);
    baudButton(115200);
    baudButton(230400);
    baudButton(460800);
  }
</script>
</body></html>
