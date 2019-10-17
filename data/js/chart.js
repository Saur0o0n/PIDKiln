var ctx = document.getElementById('ProgramChart').getContext('2d');
var chart_update_id;

var chartColors = {
	red: 'rgb(255, 99, 132)',
	blue: 'rgb(54, 162, 235)',
	yel: 'rgb(150, 100, 0)',
	green: 'rgba(0, 170, 70, 0.1)'
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
			yAxisID: 'temperature',
			backgroundColor: 'transparent',
			borderColor: chartColors.yel,
			pointBackgroundColor: chartColors.yel,
			tension: 0.1,
			fill: false
		     }, {
			yAxisID: 'watts',
			backgroundColor: chartColors.green,
			borderColor: 'transparent',
			fill: true
		     }, {
			label: 'Running program ~PROGRAM_NAME~',
			yAxisID: 'temperature',
			backgroundColor: 'transparent',
			borderColor: chartColors.blue,
			fill: false,
			tension: 0.1,
			data: [~CHART_DATA~]
		}]
	},
	plugins: [ChartDataSource],
	options: {
		title: {
			display: true,
			text: 'Set program: ~PROGRAM_NAME~ vs actual temperature'
		},
		responsive: true,
		scales: {
			xAxes: [{
				type: 'time',
				time: {
					tooltipFormat: 'YYYY-MM-DD HH:mm',
					displayFormats: {
						millisecond: 'HH:mm:ss.SSS',
						second: 'HH:mm:ss',
						minute: 'HH:mm',
						hour: 'HH'
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
					labelString: "Temperature (C)"
				}
			},{
				id: 'watts',
				position: 'right',
				gridLines: {
					display: false
				},
				scaleLabel: {
					display: true,
					labelString: 'Watts'
				},
				ticks: {
					bounds: 'data',
					beginAtZero: true,
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
			label: 'Loaded program: ~PROGRAM_NAME~',
			tension: 0.1,
			yAxisID: 'temperature',
			backgroundColor: 'transparent',
			borderColor: chartColors.blue,
			fill: false,
			data: [~CHART_DATA~]
		}]
	},
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
					tooltipFormat: 'YYYY-MM-DD HH:mm',
					displayFormats: {
						millisecond: 'HH:mm:ss.SSS',
						second: 'HH:mm:ss',
						minute: 'HH:mm',
						hour: 'HH'
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
