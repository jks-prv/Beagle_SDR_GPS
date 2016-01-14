/*
 Copyright 2011-2012 Martin Matysiak.

 Licensed under the Apache License, Version 2.0 (the "License");
 you may not use this file except in compliance with the License.
 You may obtain a copy of the License at

 http://www.apache.org/licenses/LICENSE-2.0

 Unless required by applicable law or agreed to in writing, software
 distributed under the License is distributed on an "AS IS" BASIS,
 WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
 See the License for the specific language governing permissions and
 limitations under the License.
*/
var DayNightOverlay = function(a) {
    this.canvas_ = null;
    this.fillColor_ = "rgba(0,0,0,0.5)";
    this.date_ = this.id_ = null;
    if ("object" == typeof a) {
        if ("undefined" != typeof a.fillColor)
            this.fillColor_ = a.fillColor;
        if ("undefined" != typeof a.id)
            this.id_ = a.id;
        if ("undefined" != typeof a.date)
            this.date_ = a.date;
        "undefined" != typeof a.map && this.setMap(a.map)
    }
};
DayNightOverlay.prototype = new google.maps.OverlayView;
DayNightOverlay.NORTH_ = new google.maps.LatLng(85, 0);
DayNightOverlay.SOUTH_ = new google.maps.LatLng( - 85, 0);
DayNightOverlay.prototype.onAdd = function() {
    this.canvas_ = document.createElement("canvas");
    this.canvas_.style.position = "absolute";
    if (this.id_)
        this.canvas_.id = this.id_;
    this.getPanes().overlayLayer.appendChild(this.canvas_)
};
DayNightOverlay.prototype.onRemove = function() {
    this.canvas_.parentNode.removeChild(this.canvas_);
    this.canvas_ = null
};
DayNightOverlay.prototype.draw = function() {
    var a = this.getProjection(), b = this.getWorldDimensions_(a), a = this.getVisibleDimensions_(a, 250);
    if (3 > this.getMap().getZoom())
        a.x = b.x - b.width, a.y = b.y, a.width = 3 * b.width, a.height = b.height;
    this.canvas_.style.left = a.x + "px";
    this.canvas_.style.top = a.y + "px";
    this.canvas_.style.width = a.width + "px";
    this.canvas_.style.height = a.height + "px";
    this.canvas_.width = a.width;
    this.canvas_.height = a.height;
    var c = this.canvas_.getContext("2d");
    c.clearRect(0, 0, a.width, a.height);
    var b = this.createTerminatorFunc_(a,
    b), d = this.isNorthernSun_(this.date_ ? this.date_ : new Date);
    c.fillStyle = this.fillColor_;
    c.beginPath();
    c.moveTo(0, d ? a.height : 0);
    for (var e = 0; e < a.width; e++)
        c.lineTo(e, b(e));
    c.lineTo(a.width, d ? a.height : 0);
    c.fill()
};
DayNightOverlay.prototype.setDate = function(a) {
    this.date_ = a;
    null !== this.canvas_ && this.draw()
};
DayNightOverlay.prototype.getWorldDimensions_ = function(a) {
    var b = a.fromLatLngToDivPixel(DayNightOverlay.NORTH_), c = a.fromLatLngToDivPixel(DayNightOverlay.SOUTH_), a = a.getWorldWidth();
    return {
        x: b.x - a / 2,
        y: b.y,
        width: a,
        height: c.y - b.y
    }
};
DayNightOverlay.prototype.getVisibleDimensions_ = function(a, b) {
    var c = a.fromLatLngToDivPixel(this.getMap().getBounds().getNorthEast()), d = a.fromLatLngToDivPixel(this.getMap().getBounds().getSouthWest());
    return {
        x: d.x - b,
        y: c.y - b,
        width: c.x - d.x + 2 * b,
        height: d.y - c.y + 2 * b
    }
};
DayNightOverlay.prototype.createTerminatorFunc_ = function(a, b) {
    var c = this.date_ ? this.date_: new Date, d = 2 * Math.PI, e = b.height / 2, f = a.height, g = a.x - b.x, h = a.y - b.y, i = d / b.width, j = b.height / Math.PI, k = (3600 * c.getUTCHours() + 60 * c.getUTCMinutes() + c.getUTCSeconds() + 43200) * (d / 86400), l = this.getDayOfYear_(c), c = this.getDayOfYear_(new Date(Date.UTC(c.getFullYear(), 2, 20))), m = 23.44 * Math.PI / 90, n = Math.sin(d * (l - c) / 365) * m;
    return function(a) {
        a = Math.atan( - Math.cos((a + g) * i + k) / Math.tan(n));
        a = e + a * j;
        return Math.min(f, Math.max(0, a -
        h))
    }
};
DayNightOverlay.prototype.isNorthernSun_ = function(a) {
    var b = new Date(Date.UTC(a.getFullYear(), 2, 19)), c = new Date(Date.UTC(a.getFullYear(), 8, 18));
    return a.getTime() > b.getTime() && a.getTime() <= c.getTime()
};
DayNightOverlay.prototype.getDayOfYear_ = function(a) {
    var b = new Date(Date.UTC(a.getFullYear(), 0, 1));
    return Math.ceil((a.getTime() - b.getTime()) / 864E5)
};
