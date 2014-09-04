var plot;

function OgChartPlot(title, hypo, hyper, series)
{
  var overlay = {
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

  plot = $.jqplot('chart', series, {
    title: title,
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
    canvasOverlay: overlay,
  });

  $(window).resize(function() {
    plot.replot({resetAxis: true});
  });
}

function OgChartRePlot(series)
{
  plot.replot({resetAxis: true, data: series});
}
