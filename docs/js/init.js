function setupBody(html) {
	var bodyElement = document.getElementById('body');
	bodyElement.innerHTML  = '<div id="header"><h1 id="title"></h1><span id="controls"></span></div>';
	bodyElement.innerHTML += '<div id="stat"><div class="spinner" id="spinner"></div><div class="emscripten" id="status"></div></div>';
	bodyElement.innerHTML += '<div class="emscripten_border"><canvas class="emscripten" id="canvas" oncontextmenu="event.preventDefault()"></canvas></div>';
	bodyElement.innerHTML += '<div id="footer">' + html + '<div id="output"></div><p id="powered"></p><p id="back"><a href="index.html">&lt;</a></p></div>';
}
