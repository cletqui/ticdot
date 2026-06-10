var Clay = require("pebble-clay");

var COLOR_OPTIONS = [
  { label: "Orange", value: 0 },
  { label: "Red", value: 1 },
  { label: "Green", value: 2 },
  { label: "Blue", value: 3 },
  { label: "Cyan", value: 4 },
  { label: "Yellow", value: 5 },
  { label: "Magenta", value: 6 },
  { label: "White", value: 7 },
  { label: "Light Gray", value: 8 },
];

function colorSelect(messageKey, label, defaultValue) {
  return {
    type: "select",
    messageKey: messageKey,
    label: label,
    defaultValue: defaultValue,
    options: COLOR_OPTIONS,
  };
}

var clayConfig = [
  { type: "heading", defaultValue: "TicDot" },

  {
    type: "section",
    items: [
      { type: "heading", defaultValue: "Display" },
      {
        type: "toggle",
        messageKey: "ShowBatteryDots",
        label: "Battery Dots",
        defaultValue: true,
      },
      {
        type: "toggle",
        messageKey: "ShowStepDots",
        label: "Step Dots",
        defaultValue: true,
      },
      {
        type: "toggle",
        messageKey: "ShowDateDots",
        label: "Date Dots",
        defaultValue: true,
      },
      {
        type: "toggle",
        messageKey: "ShowMonthDots",
        label: "Month Dots",
        defaultValue: true,
      },
      {
        type: "toggle",
        messageKey: "ShowWeekdayDot",
        label: "Weekday Dot",
        defaultValue: true,
      },
      {
        type: "toggle",
        messageKey: "ShowAlarmDot",
        label: "Alarm Dot",
        defaultValue: true,
      },
      {
        type: "toggle",
        messageKey: "ShowNotifDot",
        label: "Notification Dot",
        defaultValue: false,
      },
      {
        type: "toggle",
        messageKey: "ShowEventDot",
        label: "Event Dot",
        defaultValue: true,
      },
      {
        type: "toggle",
        messageKey: "ShowHrDot",
        label: "Heart Rate Dot",
        defaultValue: true,
      },
      {
        type: "toggle",
        messageKey: "ShowActivityDot",
        label: "Activity Dot",
        defaultValue: true,
      },
    ],
  },

  {
    type: "section",
    items: [
      { type: "heading", defaultValue: "Steps" },
      {
        type: "slider",
        messageKey: "StepGoal",
        label: "Daily Goal",
        defaultValue: 10000,
        min: 1000,
        max: 30000,
        step: 1000,
      },
      colorSelect("OverGoalColor1", "1st Over-Goal Color", 0),
      colorSelect("OverGoalColor2", "2nd Over-Goal Color", 4),
    ],
  },

  {
    type: "section",
    items: [
      { type: "heading", defaultValue: "Clock Hands" },
      colorSelect("HourColor", "Hour Hand", 1),
      colorSelect("MinuteColor", "Minute Hand", 7),
    ],
  },

  {
    type: "section",
    items: [
      { type: "heading", defaultValue: "Bluetooth" },
      {
        type: "toggle",
        messageKey: "VibrateOnDisconnect",
        label: "Vibrate on Disconnect",
        defaultValue: true,
      },
      colorSelect("BtColor", "Connected Color", 7),
    ],
  },

  {
    type: "section",
    items: [
      { type: "heading", defaultValue: "Alarm & Events" },
      colorSelect("AlarmColor", "Alarm Dot", 7),
      colorSelect("EventColor", "Event Dot Color", 7),
    ],
  },

  {
    type: "section",
    items: [
      { type: "heading", defaultValue: "Notifications" },
      colorSelect("NotifNormalColor", "Normal Color", 7),
      colorSelect("NotifAlertColor", "Alert Color", 1),
      {
        type: "slider",
        messageKey: "NotifThreshold",
        label: "Alert Threshold",
        defaultValue: 5,
        min: 1,
        max: 20,
        step: 1,
      },
    ],
  },

  {
    type: "section",
    items: [
      { type: "heading", defaultValue: "Health" },
      colorSelect("HrColor", "Heart Rate Color", 7),
      colorSelect("HrAlertColor", "Heart Rate Alert Color", 1),
      {
        type: "slider",
        messageKey: "HrAlertBpm",
        label: "HR Alert Threshold (BPM)",
        defaultValue: 100,
        min: 60,
        max: 200,
        step: 5,
      },
      colorSelect("ActivityColor", "Activity Dot Color", 2),
    ],
  },

  {
    type: "section",
    items: [
      { type: "heading", defaultValue: "Weekday Colors" },
      colorSelect("WeekdayColor0", "Sunday", 5),
      colorSelect("WeekdayColor1", "Monday", 8),
      colorSelect("WeekdayColor2", "Tuesday", 1),
      colorSelect("WeekdayColor3", "Wednesday", 0),
      colorSelect("WeekdayColor4", "Thursday", 3),
      colorSelect("WeekdayColor5", "Friday", 2),
      colorSelect("WeekdayColor6", "Saturday", 6),
    ],
  },

  { type: "submit", defaultValue: "Save" },
];

var clay = new Clay(clayConfig);
