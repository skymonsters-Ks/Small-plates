document.getElementById('page_title').innerHTML = global_data_title;
document.getElementById('title').innerHTML = global_data_title;
document.getElementById('powered').innerHTML = 'powered by <a href="http://hsp.tv/make/hsp3dish.html" target="_blank">hsp3dish.js</a> / OpenHSP'

var statusElement = document.getElementById('status');
var statElement = document.getElementById('stat');
var controlsElement = document.getElementById('controls');
var outputElement = document.getElementById('output');

controlsElement.innerHTML = '<input type="button" value="Fullscreen" onclick="Module.requestFullScreen(0, 0)">';

var Module = {

	TOTAL_MEMORY: 1024 * 1024 * global_data_env.mem,
	preRun: [],
	postRun: [],

	print: function(text) {
		if (text) {
			outputElement.innerHTML += text + '<br>';
			// outputElement.scrollTop = outputElement.scrollHeight; // focus on bottom
			console.log(text);
		}
	},

	printErr: function(text) {
		// text = Array.prototype.slice.call(arguments).join(' ');
		if (0) { // XXX disabled for safety typeof dump == 'function') {
			dump(text + '\n'); // fast, straight to the real console
		} else {
			console.error(text);
		}
	},

	canvas: document.getElementById('canvas'),

	setStatus: function(text) {
		if (!this.pretext) this.pretext = '';
		if (text === this.pretext) return;
		this.pretext = text;
		if (!text) {
			statElement.style.display = 'none';
			outputElement.style.display = 'block';
			if (outputElement.textContent.indexOf('INIT') >= 0) {
				controlsElement.style.display = 'inline-block';
			}
		} else {
			console.log(text);
		}
		statusElement.innerHTML = text;
	},

	totalDependencies: 0,

	monitorRunDependencies: function(left) {
		this.totalDependencies = Math.max(this.totalDependencies, left);
		Module.setStatus(left ? 'Preparing... (' + (this.totalDependencies-left) + '/' + this.totalDependencies + ')' : 'All downloads complete.');
	},

	arguments: [global_data_file[0]]
};

Module.setStatus('Downloading...');

Module.preRun.push(function() {
	ENV.HSP_WX = String(global_data_env.wx);
	ENV.HSP_WY = String(global_data_env.wy);
	ENV.HSP_SX = String(global_data_env.wx * global_data_env.rate);
	ENV.HSP_SY = String(global_data_env.wy * global_data_env.rate);
	ENV.HSP_LIMIT_STEP = String(global_data_env.step);
	ENV.HSP_CAPTURE_KEY = String(global_data_env.cap);
	ENV.HSP_AUTOSCALE = '0';
	ENV.HSP_FPS = '0';
});

(function() {

	function runWithFS() {
		
		var loadedDataNum = 0;
		var dataNum = global_data_file.length;
		
		function loadData(id) {
			var name = global_data_file[id];
			var xhr = new XMLHttpRequest();
			Module.addRunDependency('data_' + name);
			xhr.open('GET', 'data/' + name, true);
			xhr.responseType = 'arraybuffer';
			xhr.overrideMimeType('application/octet-stream');
			xhr.onprogress = function(e) {
				Module.setStatus('Downloading data... (' + loadedDataNum + '/' + dataNum + ')');
			};
			xhr.onload = function(e) {
				if (this.status == 200) {
					var stream = FS.open(name, 'w');
					var data = new Uint8Array(this.response);
					var size = e.total;
					FS.write(stream, data, 0, size, 0);
					FS.close(stream);
					console.log('Loaded ' + name + ' ' + size + 'byte');
				} else {
					console.error('Failed to load ' + name);
				}
				loadedDataNum++;
				Module.removeRunDependency('data_' + name);
			};
			xhr.send();
		}
		
		for (var i = 0; i < dataNum; i++) loadData(i);

	}
	
	if (Module.calledRun) {
		runWithFS();
	} else {
		Module.preRun.push(runWithFS); // FS is not initialized yet, wait for it
	}

})();
