if (!global_data_env.wx) global_data_env.wx = 640;
if (!global_data_env.wy) global_data_env.wy = 480;
if (!global_data_env.rate) global_data_env.rate = 1;
if (!global_data_env.step) global_data_env.step = 5000;
if (!global_data_env.mem) global_data_env.mem = 32;

document.write('<meta charset="utf-8">');
document.write('<meta http-equiv="Content-Type" content="text/html; charset=utf-8">');
document.write('<meta name="viewport" content="width=' + (global_data_env.wx * global_data_env.rate + 120) + '">');
document.write('<title id="page_title"></title>');