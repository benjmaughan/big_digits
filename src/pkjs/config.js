/*
 * Clay settings page definition.
 *
 * Clay turns this array into the settings page shown in the Pebble phone app.
 * Each "messageKey" must match an entry in package.json -> pebble.messageKeys
 * and the MESSAGE_KEY_* names used in src/c/myface.c.
 */
module.exports = [
  { "type": "heading", "defaultValue": "Big Digits" },

  /* ---- Layout ---- */
  {
    "type": "section",
    "items": [
      { "type": "heading", "defaultValue": "Layout" },
      {
        "type": "select",
        "messageKey": "Layout",
        "label": "Arrangement",
        "defaultValue": "2",
        "options": [
          { "label": "Battery under time, date at bottom", "value": "0" },
          { "label": "Vertical battery bar at right",      "value": "1" },
          { "label": "Vertical bar and vertical date",     "value": "2" }
        ]
      },
      {
        "type": "select",
        "messageKey": "DateFormat",
        "label": "Date format",
        "description": "Numeric shows day | month | year with bars between the fields.",
        "defaultValue": "1",
        "options": [
          { "label": "Text (Fri Nov 06)",       "value": "0" },
          { "label": "Numeric (06 | 11 | 26)",  "value": "1" }
        ]
      }
    ]
  },

  /* ---- Appearance ---- */
  {
    "type": "section",
    "items": [
      { "type": "heading", "defaultValue": "Appearance" },
      {
        "type": "select",
        "messageKey": "ColorScheme",
        "label": "Color scheme",
        "description": "Standard: white on black with a color-coded battery. High contrast: pure white on black. Classic LCD: black on pale yellow-green.",
        "defaultValue": "0",
        "options": [
          { "label": "Standard",      "value": "0" },
          { "label": "High contrast", "value": "1" },
          { "label": "Classic LCD",   "value": "2" }
        ]
      },
      {
        "type": "toggle",
        "messageKey": "DigitGaps",
        "label": "Gaps between digit segments",
        "description": "Pointed, LCD-style segments with small gaps.",
        "defaultValue": true
      },
      {
        "type": "toggle",
        "messageKey": "SegmentedBattery",
        "label": "Segmented battery bar",
        "description": "10 blocks, each full or hollow, in 10% steps.",
        "defaultValue": true
      }
    ]
  },

  /* ---- Phone battery ---- */
  {
    "type": "section",
    "items": [
      { "type": "heading", "defaultValue": "Phone battery" },
      {
        "type": "toggle",
        "messageKey": "PhoneBattery",
        "label": "Show phone battery bar",
        "description": "A thinner bar beside the watch battery bar (below it, or to its right). Hidden while the phone is disconnected or if the phone app doesn't report its battery level.",
        "defaultValue": true
      },
      {
        "type": "toggle",
        "messageKey": "PhoneSegmented",
        "label": "Segmented phone battery bar",
        "description": "10 blocks, each full or hollow, in 10% steps.",
        "defaultValue": true
      }
    ]
  },

  /* ---- Bluetooth ---- */
  {
    "type": "section",
    "items": [
      { "type": "heading", "defaultValue": "Bluetooth" },
      {
        "type": "select",
        "messageKey": "BtVibrate",
        "label": "Vibrate",
        "description": "The disconnected icon always shows while Bluetooth is down. Vibration is skipped during Quiet Time.",
        "defaultValue": "2",
        "options": [
          { "label": "Off",                          "value": "0" },
          { "label": "When disconnected",            "value": "1" },
          { "label": "When disconnected and reconnected", "value": "2" }
        ]
      }
    ]
  },

  { "type": "submit", "defaultValue": "Save" }
];
