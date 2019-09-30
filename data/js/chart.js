var ctx = document.getElementById('ProgramChart').getContext('2d');
var chart_update_id;

var chartColors = {
	red: 'rgb(255, 99, 132)',
	blue: 'rgb(54, 162, 235)'
};

var color = Chart.helpers.color;
var config_with = {
	type: 'line',
	data: {
		datasets: [{
			yAxisID: 'temperature',
			backgroundColor: 'transparent',
			borderColor: chartColors.red,
			pointBackgroundColor: chartColors.red,
			tension: 0.1,
			fill: false
		     }, {
			label: 'Program ready to run',
			tension: 0.1,
			backgroundColor: 'transparent',
			borderColor: window.chartColors.blue,
			fill: false,
			data: [~CHART_DATA~]
		}]
	},
	plugins: [ChartDataSource],
	options: {
		title: {
			display: true,
			text: 'Ready to run program temperature/time graph'
		},
		responsive: true,
		scales: {
			xAxes: [{
				type: 'time',
				time: {
					// round: 'day'
					tooltipFormat: 'll HH:mm',
					displayFormats: {
						minute: 'hh:mm'
					},
//					unit: 'minute'
				},
				scaleLabel: {
					display: true,
					labelString: 'Date'
				}
			}],
			yAxes: [{
				id: 'temperature',
				gridLines: {
					display: true
				},
				scaleLabel: {
					display: true,
					labelString: 'Temperature (&deg;C)'
				}
			}]
		},
		plugins: {
			datasource: {
				type: 'csv',
				url: '~LOG_FILE~',
				delimiter: ',',
				rowMapping: 'index',
				datasetLabels: true,
				indexLabels: true
			}
		}
	}
};


var config_without = {
	type: 'line',
	data: {
		datasets: [{
			label: 'Running program',
			tension: 0.1,
			backgroundColor: 'transparent',
			borderColor: window.chartColors.blue,
			fill: false,
			data: [~CHART_DATA~]
		}]
	},
	options: {
		title: {
			display: true,
			text: 'Running program vs actual temperature'
		},
		responsive: true,
		scales: {
			xAxes: [{
				type: 'time',
				time: {
					// round: 'day'
					tooltipFormat: 'll HH:mm',
					displayFormats: {
						minute: 'hh:mm'
					},
//					unit: 'minute'
				},
				scaleLabel: {
					display: true,
					labelString: 'Date'
				}
			}],
			yAxes: [{
				id: 'temperature',
				gridLines: {
					display: true
				},
				scaleLabel: {
					display: true,
					labelString: 'Temperature (&deg;C)'
				}
			}]
		},
	}
};

var chart = new Chart(ctx, ~CONFIG~);

function chart_update(){
//  console.log("Updating chart");
  chart.update();
  chart_update_id=setTimeout(chart_update, 30000);
}
