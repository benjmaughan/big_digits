module.exports = [
  { "type": "heading", "defaultValue": "Big Digits" },
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
        "description": "Draw each digit segment separately, LCD-style.",
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
