if (!global_data_env.wx) global_data_env.wx = 640;
if (!global_data_env.wy) global_data_env.wy = 480;
if (!global_data_env.rate) global_data_env.rate = 1;
if (!global_data_env.step) global_data_env.step = 5000;
if (!global_data_env.mem) global_data_env.mem = 32;

document.write('<meta charset="utf-8">');
document.write('<meta http-equiv="Content-Type" content="text/html; charset=utf-8">');
document.write('<meta name="viewport" content="width=' + (global_data_env.wx * global_data_env.rate + 120) + '">');
document.write('<title id="page_title"></title>');

function setupBody(html) {
	var bodyElement = document.getElementById('body');
	bodyElement.innerHTML  = '<div id="header"><h1 id="title"></h1><span id="controls"></span></div>';
	bodyElement.innerHTML += '<div id="stat"><div class="spinner" id="spinner"></div><div class="emscripten" id="status"></div></div>';
	bodyElement.innerHTML += '<div class="emscripten_border"><canvas class="emscripten" id="canvas" oncontextmenu="event.preventDefault()"></canvas></div>';
	bodyElement.innerHTML += html;
	bodyElement.innerHTML += '<div id="footer"><div id="output"></div><p id="powered"></p><p id="back"><a href="index.html">&lt;</a></p></div>';
}
