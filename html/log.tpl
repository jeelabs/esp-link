  <div id="main">
    <div class="header">
      <h1>Debug Log</h1>
    </div>

    <div class="content">
      <div class="pure-g">
        <div class="pure-u-1-5">
          <p style="padding-top: 0.4em;">
            <a id="refresh-button" class="pure-button button-primary" href="#">Refresh</a>
          </p>
        </div>
        <p class="pure-u-4-5">
          The debug log shows the 1024 last characters printed by the esp-link software itself to
          its own debug log.
        </p>
      </div>
      <pre id="console" class="console" style="margin-top: 0px;"></pre>
    </div>
  </div>
</div>

<script src="ui.js"></script>
<script type="text/javascript">console_url = "/log/text"</script>
<script src="console.js"></script>
<script type="text/javascript">
  window.onload = function() {
    fetchText(100, false);

    $("#refresh-button").addEventListener("click", function(e) {
      e.preventDefault();
      fetchText(100, false);
    });
  }
</script>
</body></html>
