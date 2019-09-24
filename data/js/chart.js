var ctx = document.getElementById('myChart').getContext('2d');


var chartColors = {
	red: 'rgb(255, 99, 132)',
	blue: 'rgb(54, 162, 235)'
};

var color = Chart.helpers.color;
var config = {
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
			label: 'Prepared program',
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
			text: 'Running program vs actual temperature'
		},
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
				url: 'log/test.csv',
				delimiter: ',',
				rowMapping: 'index',
				datasetLabels: true,
				indexLabels: true
			}
		}
	}
};

var chart = new Chart(ctx, config);
