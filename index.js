// Script for outputting the calibration data from HoloPlay.js library
// Imitates browser environment

import { createRequire } from "module";
const require = createRequire(import.meta.url);
globalThis.THREE = require('three');
globalThis.dom = require("mock-dom-resources")();
globalThis.document = dom.document;
globalThis.window = dom.window;
globalThis.ElementPrototype = Object.create(document.createElement('A'));
globalThis.Element = function ()
{
}
Object.assign(Element.prototype, ElementPrototype)
globalThis.alert = function (message)
{
    console.log("alert: " + message);
}
globalThis.Holoplay = require('holoplay');
console.log(JSON.stringify(await Holoplay.Calibration.getCalibration()));
process.exit()