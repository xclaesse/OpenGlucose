function OgMainWindow()
{
  var plots = {};

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

  this.updateDevice = function(device) {
    this.removeDevice(device.id);

    $("body").append("<div id='" + device.id + "'/>");
    $("#" + device.id).addClass("device-container");
    $("#" + device.id).append("<h1>" + device.name + "</h1>");
    updateNoDeviceMessage();

    if (device.refreshing) {
      $("#" + device.id).append("<p>Fetching device information</p>");
      return;
    }

    if (device.failed) {
      $("#" + device.id).append("<p>Failed to fetch device information</p>");
      return;
    }

    $("#" + device.id).append("<p>Serial Number: " + device.sn + "</p>");
    $("#" + device.id).append("<dir id='chart-" + device.id + "'/>");
    $("#chart-" + device.id).addClass("chart");

    plots[device.id] = $.jqplot('chart-' + device.id, [device.data], {
      title: 'Glycemias',
      series: [{
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
          tickOptions: {angle: 30},
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

  this.removeDevice = function(id) {
    $("#" + id).remove();
    if (id in plots) {
      plots[id].destroy();
      delete plots[id];
    }
    updateNoDeviceMessage();
  };

  this.documentReady = function() {
    $("body").append("<h1 id='no-device'>No device connected</h1>");
    updateNoDeviceMessage();
  };

  this.resize = function() {
    for (var key in plots)
      plots[key].replot({resetAxis: true});
  };
}

var og = new OgMainWindow();

$(document).ready(function() {
  og.documentReady();
});

$(window).resize(function() {
  og.resize();
});
