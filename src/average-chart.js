var plot;

function OgChartPlot(title, data)
{
  plot = jQuery.jqplot ('chart', [data], {
    seriesDefaults: {
      renderer: jQuery.jqplot.PieRenderer,
      rendererOptions: {
        showDataLabels: true
      }
    },
    legend: {
      show: true,
      location: 'e'
    }
  });

  $(window).resize(function() {
    plot.replot({resetAxis: true});
  });
}

function OgChartRePlot(data)
{
  plot.replot({resetAxis: true, data: [data]});
}
