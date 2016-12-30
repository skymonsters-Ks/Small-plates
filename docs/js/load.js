var statusElement = document.getElementById('status');
var progressElement = document.getElementById('progress');
var spinnerElement = document.getElementById('spinner');
var controlsElement = document.getElementById('controls');

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
			}
		}
		statusElement.innerHTML = text;
	},
	totalDependencies: 0,
	monitorRunDependencies: function(left) {
		this.totalDependencies = Math.max(this.totalDependencies, left);
		Module.setStatus(left ? 'Preparing... (' + (this.totalDependencies-left) + '/' + this.totalDependencies + ')' : 'All downloads complete.');
	},
	arguments: [global_data_obj.ax+".ax"]
};


Module.setStatus('Downloading...');


/* block.js */
var Module;
if (typeof Module === 'undefined') Module = eval('(function() { try { return Module || {} } catch(e) { return {} } })()');
if (!Module.expectedDataFileDownloads) {
	Module.expectedDataFileDownloads = 0;
	Module.finishedDataFileDownloads = 0;
}
Module.expectedDataFileDownloads++;
(function() {

	var PACKAGE_PATH;
	if (typeof window === 'object') {
		PACKAGE_PATH = window['encodeURIComponent'](window.location.pathname.toString().substring(0, window.location.pathname.toString().lastIndexOf('/')) + '/');
	} else {
		// worker
		PACKAGE_PATH = encodeURIComponent(location.pathname.toString().substring(0, location.pathname.toString().lastIndexOf('/')) + '/');
	}
	var PACKAGE_NAME = global_data_obj.fn+'.data';
	var REMOTE_PACKAGE_NAME = (Module['filePackagePrefixURL'] || '') + global_data_obj.fn+'.data';
	var REMOTE_PACKAGE_SIZE = global_data_obj.sz;
	var PACKAGE_UUID = '51f84d03-0c74-4552-ac13-924e79e76977';
  
	function fetchRemotePackage(packageName, packageSize, callback, errback) {
		var xhr = new XMLHttpRequest();
		xhr.open('GET', packageName, true);
		xhr.responseType = 'arraybuffer';
		xhr.onprogress = function(event) {
			var url = packageName;
			var size = packageSize;
			if (event.total) size = event.total;
			if (event.loaded) {
				if (!xhr.addedTotal) {
					xhr.addedTotal = true;
					if (!Module.dataFileDownloads) Module.dataFileDownloads = {};
					Module.dataFileDownloads[url] = {
						loaded: event.loaded,
						total: size
					};
				} else {
					Module.dataFileDownloads[url].loaded = event.loaded;
				}
				var total = 0;
				var loaded = 0;
				var num = 0;
				for (var download in Module.dataFileDownloads) {
					var data = Module.dataFileDownloads[download];
					total += data.total;
					loaded += data.loaded;
					num++;
				}
				total = Math.ceil(total * Module.expectedDataFileDownloads/num);
				if (Module['setStatus']) Module['setStatus']('Downloading data... (' + loaded + '/' + total + ')');
			} else if (!Module.dataFileDownloads) {
				if (Module['setStatus']) Module['setStatus']('Downloading data...');
			}
		};
		xhr.onload = function(event) {
			var packageData = xhr.response;
			callback(packageData);
		};
		xhr.send(null);
	};

	function handleError(error) {
		console.error('package error:', error);
	};

	var fetched = null, fetchedCallback = null;
	fetchRemotePackage(REMOTE_PACKAGE_NAME, REMOTE_PACKAGE_SIZE, function(data) {
		if (fetchedCallback) {
			fetchedCallback(data);
			fetchedCallback = null;
		} else {
			fetched = data;
		}
	}, handleError);
    
	function runWithFS() {

		function assert(check, msg) {
			if (!check) throw msg + new Error().stack;
		}

		function DataRequest(start, end, crunched, audio) {
			this.start = start;
			this.end = end;
			this.crunched = crunched;
			this.audio = audio;
		}
		DataRequest.prototype = {
			requests: {},
			open: function(mode, name) {
				this.name = name;
				this.requests[name] = this;
				Module['addRunDependency']('fp ' + this.name);
			},
			send: function() {},
			onload: function() {
				var byteArray = this.byteArray.subarray(this.start, this.end);

				this.finish(byteArray);

			},
			finish: function(byteArray) {
				var that = this;
				Module['FS_createPreloadedFile'](this.name, null, byteArray, true, true, function() {
					Module['removeRunDependency']('fp ' + that.name);
				}, function() {
					if (that.audio) {
						Module['removeRunDependency']('fp ' + that.name); // workaround for chromium bug 124926 (still no audio with this, but at least we don't hang)
					} else {
						Module.printErr('Preloading file ' + that.name + ' failed');
					}
				}, false, true); // canOwn this data in the filesystem, it is a slide into the heap that will never change
				this.requests[this.name] = null;
			},
		};

		Module.preRun.push(function() {
			ENV.HSP_WX = global_data_obj.wx;
			ENV.HSP_WY = global_data_obj.wy;
			ENV.HSP_SX = global_data_obj.sx;
			ENV.HSP_SY = global_data_obj.sy;
			ENV.HSP_AUTOSCALE = global_data_obj.as;
			ENV.HSP_FPS = global_data_obj.fps;
			ENV.HSP_LIMIT_STEP = global_data_obj.step;
		});

		for (var i = 0; i < global_data_obj.n; i++) {
			new DataRequest(global_data_obj.s[i], global_data_obj.e[i], global_data_obj.c[i], global_data_obj.a[i]).open('GET', '/'+global_data_obj.f[i]);
		}

		function processPackageData(arrayBuffer) {
			Module.finishedDataFileDownloads++;
			assert(arrayBuffer, 'Loading data file failed.');
			var byteArray = new Uint8Array(arrayBuffer);
			var curr;

			// copy the entire loaded file into a spot in the heap. Files will refer to slices in that. They cannot be freed though.
			var ptr = Module['_malloc'](byteArray.length);
			Module['HEAPU8'].set(byteArray, ptr);
			DataRequest.prototype.byteArray = Module['HEAPU8'].subarray(ptr, ptr+byteArray.length);

			for (var i = 0; i < global_data_obj.n; i++) {
				DataRequest.prototype.requests["/"+global_data_obj.f[i]].onload();
			}

			Module['removeRunDependency']('datafile_'+global_data_obj.fn+'.data');

		};
		Module['addRunDependency']('datafile_'+global_data_obj.fn+'.data');
  
		if (!Module.preloadResults) Module.preloadResults = {};
  
		Module.preloadResults[PACKAGE_NAME] = {fromCache: false};
		if (fetched) {
			processPackageData(fetched);
			fetched = null;
		} else {
			fetchedCallback = processPackageData;
		}
    
	}
	if (Module['calledRun']) {
		runWithFS();
	} else {
		if (!Module['preRun']) Module['preRun'] = [];
		Module["preRun"].push(runWithFS); // FS is not initialized yet, wait for it
	}

})();
