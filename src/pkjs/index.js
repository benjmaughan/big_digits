var Clay = require('pebble-clay');
var clayConfig = require('./config');
var clay = new Clay(clayConfig);

/*
 * Phone battery level.
 *
 * Sends the phone's battery charge (0-100) to the watch as
 * PhoneBatteryLevel: once when the watchface starts, whenever the level
 * changes, and whenever the watch asks for it (PhoneBatteryRequest, sent
 * after the Bluetooth connection comes back).
 *
 * This relies on the Battery Status API (navigator.getBattery) being
 * available in the phone app's JavaScript environment. If it isn't, nothing
 * is sent and the watch simply doesn't show the phone bar.
 */
var battery = null;      /* BatteryManager, once we have one */
var lastSent = null;     /* last level sent, to avoid repeats */

function sendLevel(force) {
  if (!battery) return;
  var level = Math.round(battery.level * 100);
  if (!force && level === lastSent) return;

  Pebble.sendAppMessage({ PhoneBatteryLevel: level },
    function () { lastSent = level; },
    function () { console.log('Phone battery: send failed'); });
}

function startBattery() {
  if (typeof navigator === 'undefined' ||
      typeof navigator.getBattery !== 'function') {
    console.log('Phone battery: not available on this phone');
    return;
  }
  navigator.getBattery().then(function (b) {
    battery = b;
    b.addEventListener('levelchange', function () { sendLevel(false); });
    sendLevel(true);
  }, function (err) {
    console.log('Phone battery: unavailable (' + err + ')');
  });
}

Pebble.addEventListener('ready', function () {
  startBattery();
});

Pebble.addEventListener('appmessage', function (e) {
  if (e.payload && e.payload.PhoneBatteryRequest !== undefined) {
    sendLevel(true);
  }
});
