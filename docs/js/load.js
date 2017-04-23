var statusElement = document.getElementById('status');
var statElement = document.getElementById('stat');
var controlsElement = document.getElementById('controls');
var outputElement = document.getElementById('output');

var Module = {
	TOTAL_MEMORY: 1024*1024*30,
	preRun: [],
	postRun: [],
	print: (function() {
		var element = document.getElementById('output');
		if (element) element.value = ''; // clear browser cache
		return function(text) {
			text = Array.prototype.slice.call(arguments).join(' ');
			console.log(text);
			if (element) {
				element.value += text + "\n";
				// element.scrollTop = element.scrollHeight; // focus on bottom
			}
		};
	})(),
	printErr: function(text) {
		text = Array.prototype.slice.call(arguments).join(' ');
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
			controlsElement.style.display = 'inline-block';
			outputElement.style.display = 'block';
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
	arguments: [global_data_obj.f[0]]
};

Module.setStatus('Downloading...');

Module.preRun.push(function() {
	ENV.HSP_WX = global_data_obj.wx;
	ENV.HSP_WY = global_data_obj.wy;
	ENV.HSP_SX = global_data_obj.sx;
	ENV.HSP_SY = global_data_obj.sy;
	ENV.HSP_AUTOSCALE = global_data_obj.as;
	ENV.HSP_FPS = global_data_obj.fps;
	ENV.HSP_LIMIT_STEP = global_data_obj.step;
});

(function() {

	function runWithFS() {
		
		var loadedDataNum = 0;
		var dataNum = global_data_obj.f.length;
		
		function loadData(id) {
			var name = global_data_obj.f[id];
			var xhr = new XMLHttpRequest();
			Module["addRunDependency"]('data_' + name);
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
					console.log('Failed to load ' + name);
				}
				loadedDataNum++;
				Module['removeRunDependency']('data_' + name);
			};
			xhr.send();
		}
		
		for (var i = 0; i < dataNum; i++) loadData(i);

	}
	
	if (Module['calledRun']) {
		runWithFS();
	} else {
		if (!Module['preRun']) Module['preRun'] = [];
		Module["preRun"].push(runWithFS); // FS is not initialized yet, wait for it
	}

})();
