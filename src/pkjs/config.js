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
