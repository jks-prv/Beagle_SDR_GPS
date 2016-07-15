// Copyright (c) 2016 John Seamons, ZL/KF6VO

var cfg = { };

function ext_switch_to_client(ext_name, ext_ws)
{
	//console.log('SET ext_switch_to_client='+ ext_name +' rx_chan='+ rx_chan);
	ext_ws.send('SET ext_switch_to_client='+ ext_name +' rx_chan='+ rx_chan);
}

var extint_ws;

function ext_connect_server(ext_name, recv_func)
{
	extint_ws = open_websocket("EXT", timestamp, function(data) {
		var stringData = arrayBufferToString(data);
		var param = stringData.substring(4).split("=");

		switch (param[0]) {

			case "keepalive":
				break;

			case "ext_client_init":
				// rx_chan is set globally by waterfall code
				// FIXME better way to handle this?
				extint_ws.send('SET ext_client_reply='+ ext_name +' rx_chan='+ rx_chan);
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
	setTimeout(function() { setInterval(function() { extint_ws.send("SET keepalive") }, 5000) }, 5000);
	return extint_ws;
}

function ext_panel_show(controls_html, data_html, show_func)
{
	extint_panel_show(controls_html, data_html, show_func);
}


// private

var extint_current_ext_name = null;

function extint_blur_prev()
{
	if (extint_current_ext_name != null)
		w3_call(extint_current_ext_name +'_blur', null);
	if (extint_ws)
		extint_ws.send('SET ext_blur='+ rx_chan);
}

function extint_focus(ext_name)
{
	extint_current_ext_name = ext_name;
	
	console.log('extint_focus: calling '+ extint_current_ext_name +'_main()');
	w3_call(extint_current_ext_name +'_main', null);
}

function extint_select(val)
{
	extint_blur_prev();
	var i = parseInt(val)-1;
	extint_focus(ext_list[i]);
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
