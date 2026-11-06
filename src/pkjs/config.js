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
        "defaultValue": "0",
        "options": [
          { "label": "Battery under time, date at bottom", "value": "0" },
          { "label": "Vertical battery bar at right",      "value": "1" },
          { "label": "Vertical bar and vertical date",     "value": "2" }
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
        "type": "toggle",
        "messageKey": "HighContrast",
        "label": "High contrast",
        "description": "Pure white on black everywhere, no color coding, thicker battery line.",
        "defaultValue": false
      },
      {
        "type": "toggle",
        "messageKey": "DigitGaps",
        "label": "Gaps between digit segments",
        "description": "Pointed, LCD-style segments with small gaps.",
        "defaultValue": false
      },
      {
        "type": "toggle",
        "messageKey": "SegmentedBattery",
        "label": "Segmented battery bar",
        "description": "10 blocks, each full or hollow, in 10% steps.",
        "defaultValue": false
      }
    ]
  },

  { "type": "submit", "defaultValue": "Save" }
];
