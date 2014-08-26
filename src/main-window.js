function OgMainWindow()
{
  var devices = {};

  var updateNoDeviceMessage = function() {
    if ($(".device-container").length == 0)
      $("#no-device").show();
    else
      $("#no-device").hide();
  };

  var getPlotOverlay = function(hypo, hyper) {
    return {
      show: true,
      objects: [{
        rectangle: {
          ymin: 0,
          ymax: hypo,
          xminOffset: "0px",
          xmaxOffset: "0px",
          yminOffset: "0px",
          ymaxOffset: "0px",
          color: "rgba(0, 0, 255, 0.3)",
          showTooltip: true,
          tooltipFormatString: "Hypoglycemia"
        }
      },
      {
        rectangle: {
          ymin: hypo,
          ymax: hyper,
          xminOffset: "0px",
          xmaxOffset: "0px",
          yminOffset: "0px",
          ymaxOffset: "0px",
          color: "rgba(0, 255, 0, 0.3)",
          showTooltip: true,
          tooltipFormatString: "Good"
        }
      },
      {
        rectangle: {
          ymin: hyper,
          xminOffset: "0px",
          xmaxOffset: "0px",
          yminOffset: "0px",
          ymaxOffset: "0px",
          color: "rgba(255, 0, 0, 0.3)",
          showTooltip: true,
          tooltipFormatString: "Hyperglycemia"
        }
      }]
    };
  };

  var dailyAverageSeries = function(device) {
    var serie1 = [];
    var serie2 = [];

    var averages = [];
    for (var i = 0; i < 12; i++)
      averages.push({sum: 0, nVals: 0});

    var now = new Date();

    for (var i = 0; i < device.data.length; i++) {
      var diff = now - device.data[i][0];
      if (device.timeRange >= 0 && diff > device.timeRange)
        continue;

      var p = Math.trunc(device.data[i][0].getHours() / 2);
      averages[p].sum += device.data[i][1];
      averages[p].nVals++;

      var date = new Date(0, 0, 0,
          device.data[i][0].getHours(),
          device.data[i][0].getMinutes(),
          0, 0);
      serie1.push([date, device.data[i][1]]);
    }

    for (var i = 0; i < 12; i++) {
      if (averages[i].nVals > 0) {
        var date = new Date(0, 0, 0, i * 2 + 1, 0, 0, 0);
        serie2.push([date, averages[i].sum / averages[i].nVals]);
      }
    }

    return [serie1, serie2];
  };

  this.updateDevice = function(device) {
    if (device.id in devices) {
      $("#" + device.id).empty();
    } else {
      $("body").append("<div id='" + device.id + "' class='device-container'/>");
      updateNoDeviceMessage();
    }

    removeDeviceInternal(device.id);
    devices[device.id] = device;

    $("#" + device.id).append("<h1>" + device.name + "</h1>");

    if (device.refreshing) {
      $("#" + device.id).append("<p>Fetching device information</p>");
      return;
    }

    if (device.failed) {
      $("#" + device.id).append("<p>Failed to fetch device information</p>");
      return;
    }

    $("#" + device.id).append(
        "<table class='device-info-container'>" +
        "  <tr><td>Serial Number:</td><td>" + device.sn + "</td></tr>" +
        "  <tr><td>Number of results:</td><td>" + device.data.length + "</td></tr>" +
        "</table>");

    var selectors =
        "<table class='chart-period-selector'>" +
        "  <tr>" +
        "    <td><input type='button' onclick='og.changeTimeRange(\"" + device.id + "\", 1*24*60*60*1000)' value='1D'></input></td>" +
        "    <td><input type='button' onclick='og.changeTimeRange(\"" + device.id + "\", 7*24*60*60*1000)' value='1W'></input></td>" +
        "    <td><input type='button' onclick='og.changeTimeRange(\"" + device.id + "\", 30*24*60*60*1000)' value='1M'></input></td>" +
        "    <td><input type='button' onclick='og.changeTimeRange(\"" + device.id + "\", 365*24*60*60*1000)' value='1Y'></input></td>" +
        "    <td><input type='button' onclick='og.changeTimeRange(\"" + device.id + "\", -1)' value='All'></input></td>" +
        "  </tr>" +
        "</table>";
    $("#" + device.id).append(selectors);

    $("#" + device.id).append("<div id='chart-" + device.id + "'/>");
    var series = dailyAverageSeries(device);
    device.plot = $.jqplot('chart-' + device.id, series, {
      title: 'Daily average',
      series: [{
        renderer: $.jqplot.LineRenderer,
        showLine: false,
        pointLabels: {show: false},
        markerOptions: {size: 5}
      },
      {
        renderer: $.jqplot.LineRenderer,
        lineWidth: 2,
        pointLabels: {show: false},
        markerOptions: {size: 5}
      }],
      axesDefaults: {
        tickRenderer: $.jqplot.CanvasAxisTickRenderer,
      },
      axes: {
        xaxis: {
          renderer: $.jqplot.DateAxisRenderer,
          tickOptions: {
            angle: 30,
            formatString: '%H:%M',
          },
          autoscale: true,
        },
        yaxis: {
          min: 0,
          tickOptions: {angle: 30},
          autoscale: true,
        },
      },
      canvasOverlay: getPlotOverlay(70, 160),
    });
  };

  var removeDeviceInternal = function(id) {
    var device = devices[id];
    if (device && device.plot)
      device.plot.destroy();
    delete devices[id];
  }

  this.removeDevice = function(id) {
    $("#" + id).remove();
    removeDeviceInternal(id);
    updateNoDeviceMessage();
  };

  this.changeTimeRange = function(id, range) {
    var device = devices[id];
    device.timeRange = range;
    if (device.plot) {
      device.plot.replot({
        resetAxis: true,
        data: dailyAverageSeries(device)
      });
    }
  };

  this.documentReady = function() {
    $("body").append("<h1 id='no-device'>No device connected</h1>");
    updateNoDeviceMessage();
  };

  this.resize = function() {
    for (var key in devices) {
      var device = devices[key];
      if (device.plot)
        device.plot.replot({resetAxis: true});
    }
  };
}

var og = new OgMainWindow();

$(document).ready(function() {
  og.documentReady();
});

$(window).resize(function() {
  og.resize();
});
