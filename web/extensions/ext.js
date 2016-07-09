// Copyright (c) 2016 John Seamons, ZL/KF6VO

var cfg = { };

function ext_init(ext_name, ext_ws)
{
	// rx_chan is set globally by waterfall code
	ext_ws.send('SET ext_client_reply='+ ext_name +' rx_chan='+ rx_chan);
}

function ext_connect_server(recv_func)
{
	var ws = open_websocket("EXT", timestamp, function(data) {
		var stringData = arrayBufferToString(data);
		var param = stringData.substring(4).split("=");

		switch (param[0]) {

			case "keepalive":
				break;

			case "ext_cfg_json":
				cfg = JSON.parse(decodeURIComponent(param[1]));
				break;
			
			default:
				//console.log('ext WS '+ data);
				recv_func(data);
				break;
		}
	});

	// when the stream thread is established on the server it will automatically send a "SET init" to us
	setTimeout(function() { setInterval(function() { ws.send("SET keepalive") }, 5000) }, 5000);
	return ws;
}

function ext_panel_show(controls_html, data_html, show_func, hide_func)
{
	extint_panel_show(controls_html, data_html, show_func, hide_func);
}


// private

function extint_select(val)
{
	var i = parseInt(val)-1;
	console.log('extint_select: '+ i +' calling '+ ext_list[i] +'_main()');
	w3_call(ext_list[i] +'_main', null);
}

var ext_list;

function extint_list_json(param)
{
	ext_list = JSON.parse(decodeURIComponent(param));
	//console.log('### ext_list=');
	//console.log(ext_list);
}

function extint_select_menu()
{
	var s = '';
	for (var i=0; i < ext_list.length; i++) {
		s += '<option value="'+ (i+1) +'">'+ ext_list[i] +'</option>';
	}
	//console.log('extint_select_menu = '+ s);
	return s;
}
