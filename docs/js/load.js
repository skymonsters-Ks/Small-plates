var statusElement = document.getElementById('status');
var progressElement = document.getElementById('progress');
var spinnerElement = document.getElementById('spinner');
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
			// These replacements are necessary if you render to raw HTML
			//text = text.replace(/&/g, "&amp;");
			//text = text.replace(/</g, "&lt;");
			//text = text.replace(/>/g, "&gt;");
			//text = text.replace('\n', '<br>', 'g');
			console.log(text);
			if (element) {
				element.value += text + "\n";
				element.scrollTop = element.scrollHeight; // focus on bottom
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
		if (!Module.setStatus.last) Module.setStatus.last = { time: Date.now(), text: '' };
		if (text === Module.setStatus.text) return;
		var m = text.match(/([^(]+)\((\d+(\.\d+)?)\/(\d+)\)/);
		var now = Date.now();
		if (m && now - Date.now() < 30) return; // if this is a progress update, skip it if too soon
		if (m) {
			text = m[1];
			progressElement.value = parseInt(m[2])*100;
			progressElement.max = parseInt(m[4])*100;
			progressElement.hidden = false;
			spinnerElement.hidden = false;
		} else {
			progressElement.value = null;
			progressElement.max = null;
			progressElement.hidden = true;
			if (!text) {
				spinnerElement.style.display = 'none';
				controlsElement.style.display = 'inline-block';
				outputElement.style.display = 'block';
			}
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
			xhr.setRequestHeader('Accept-Encoding', 'identity');
			xhr.onprogress = function(e) {
				Module.setStatus('Downloading data... (' + loadedDataNum + '/' + dataNum + ')');
			};
			xhr.onload = function(e) {
				if (this.status == 200) {
					var stream = FS.open(name, 'w');
					var data = new Uint8Array(this.response);
					FS.write(stream, data, 0, e.total, 0);
					FS.close(stream);
					console.log('Downloaded ' + name + ' ' + xhr.getAllResponseHeaders());
				} else {
					console.log('Failed to download ' + name);
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
