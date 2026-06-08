var Clay = require('pebble-clay');

var clayConfig = [
  {
    "type": "heading",
    "defaultValue": "Tictoc Alt"
  },
  {
    "type": "section",
    "items": [
      {
        "type": "heading",
        "defaultValue": "Steps"
      },
      {
        "type": "slider",
        "messageKey": "StepGoal",
        "label": "Daily Goal",
        "defaultValue": 10000,
        "min": 1000,
        "max": 30000,
        "step": 1000
      },
      {
        "type": "toggle",
        "messageKey": "ShowStepDots",
        "label": "Show Step Dots",
        "defaultValue": true
      }
    ]
  },
  {
    "type": "section",
    "items": [
      {
        "type": "heading",
        "defaultValue": "Colors"
      },
      {
        "type": "select",
        "messageKey": "HourColor",
        "label": "Hour Hand",
        "defaultValue": 0,
        "options": [
          {"label": "Orange",   "value": 0},
          {"label": "Red",      "value": 1},
          {"label": "Green",    "value": 2},
          {"label": "Blue",     "value": 3},
          {"label": "Cyan",     "value": 4},
          {"label": "Yellow",   "value": 5},
          {"label": "Magenta",  "value": 6},
          {"label": "White",    "value": 7}
        ]
      },
      {
        "type": "select",
        "messageKey": "OverGoalColor1",
        "label": "1st Over-Goal Color",
        "defaultValue": 0,
        "options": [
          {"label": "Orange",   "value": 0},
          {"label": "Red",      "value": 1},
          {"label": "Green",    "value": 2},
          {"label": "Blue",     "value": 3},
          {"label": "Cyan",     "value": 4},
          {"label": "Yellow",   "value": 5},
          {"label": "Magenta",  "value": 6},
          {"label": "White",    "value": 7}
        ]
      },
      {
        "type": "select",
        "messageKey": "OverGoalColor2",
        "label": "2nd Over-Goal Color",
        "defaultValue": 4,
        "options": [
          {"label": "Orange",   "value": 0},
          {"label": "Red",      "value": 1},
          {"label": "Green",    "value": 2},
          {"label": "Blue",     "value": 3},
          {"label": "Cyan",     "value": 4},
          {"label": "Yellow",   "value": 5},
          {"label": "Magenta",  "value": 6},
          {"label": "White",    "value": 7}
        ]
      }
    ]
  },
  {
    "type": "section",
    "items": [
      {
        "type": "heading",
        "defaultValue": "Bluetooth"
      },
      {
        "type": "toggle",
        "messageKey": "VibrateOnDisconnect",
        "label": "Vibrate on Disconnect",
        "defaultValue": true
      }
    ]
  },
  {
    "type": "submit",
    "defaultValue": "Save"
  }
];

var clay = new Clay(clayConfig);
