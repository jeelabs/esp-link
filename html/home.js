onLoad(function() {
  makeAjaxInput("system", "description");
  makeAjaxInput("system", "name");
  fetchPins();
  getWifiInfo();
  getSystemInfo();
  bnd($("#pinform"), "submit", setPins);
});
