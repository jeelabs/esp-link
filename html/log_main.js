  console_url = "/log/text";

  onLoad(function() {
    fetchText(100, false);

    $("#refresh-button").addEventListener("click", function(e) {
      e.preventDefault();
      fetchText(100, false);
    });

    $("#reset-button").addEventListener("click", function (e) {
        e.preventDefault();
        var co = $("#console");
        co.innerHTML = "";
        ajaxSpin('POST', "/log/reset",
          function (resp) { showNotification("Resetting esp-link"); co.textEnd = 0; fetchText(2000, false); },
          function (s, st) { showWarning("Error resetting esp-link"); }
        );
    });

    ["auto", "off", "on0", "on1"].forEach(function(mode) {
      bnd($('#dbg-'+mode), "click", function(el) {
        ajaxJsonSpin('POST', "/log/dbg?mode="+mode,
          function(data) { showNotification("UART mode " + data.mode); showDbgMode(data.mode); },
          function(s, st) { showWarning("Error setting UART mode: " + st); }
        );
      });
    });

    ajaxJson('GET', "/log/dbg", function(data) { showDbgMode(data.mode); }, function() {});
  });
