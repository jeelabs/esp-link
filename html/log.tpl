  <div id="main">
    <div class="header">
      <h1><span class="esp">esp</span> link - Debug Log</h1>
    </div>

    <div class="content">
      <p>The debug log shows the 1024 last characters printed by the esp-link software itself to
      its own debug log.</p>
      <pre id="console" class="console"></pre>
    </div>
  </div>
</div>

<script src="ui.js"></script>
<script type="text/javascript">console_url = "/log/text"</script>
<script src="console.js"></script>
<script type="text/javascript">
  window.onload = function() {
    fetchText(100);
  }
</script>
</body></html>
