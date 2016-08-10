// Copyright (c) 2016 John Seamons, ZL/KF6VO

function ext_switch_to_client(ext_name, first_time, recv_func)
{
	//console.log('SET ext_switch_to_client='+ ext_name +' first_time='+ first_time +' rx_chan='+ rx_chan);
	extint_recv_func = recv_func;
	extint_ws.send('SET ext_switch_to_client='+ ext_name +' first_time='+ first_time +' rx_chan='+ rx_chan);
}

function ext_send(msg)
{
	extint_ws.send(msg);
}

function ext_panel_show(controls_html, data_html, show_func)
{
	extint_panel_show(controls_html, data_html, show_func);
}


// private

var extint_ws, extint_recv_func;

function extint_connect_server()
{
	extint_ws = open_websocket("EXT", timestamp, function(data) {
		var stringData = arrayBufferToString(data);
		var param = stringData.substring(4).split("=");

		switch (param[0]) {

			case "keepalive":
				break;

			case "ext_client_init":
				extint_focus();
				break;

			default:
				//console.log('ext WS '+ data);
				if (extint_recv_func)
					extint_recv_func(data);
				break;
		}
	});

	// when the stream thread is established on the server it will automatically send a "SET init" to us
	setTimeout(function() { setInterval(function() { extint_ws.send("SET keepalive") }, 5000) }, 5000);
	return extint_ws;
}

var extint_current_ext_name = null;

function extint_blur_prev()
{
	if (extint_current_ext_name != null)
		w3_call(extint_current_ext_name +'_blur', null);
	if (extint_ws)
		extint_ws.send('SET ext_blur='+ rx_chan);
}

function extint_focus()
{
	console.log('extint_focus: calling '+ extint_current_ext_name +'_main()');
	w3_call(extint_current_ext_name +'_main', null);
}

// called on extension menu item selection
function extint_select(val)
{
	extint_blur_prev();
	
	var i = parseInt(val)-1;
	extint_current_ext_name = extint_names[i];
	if (!extint_ws) {
		extint_ws = extint_connect_server();
	} else {
		extint_focus();
	}
}

var extint_names;

function extint_list_json(param)
{
	extint_names = JSON.parse(decodeURIComponent(param));
	//console.log('### extint_names=');
	//console.log(extint_names);
}

function extint_select_menu()
{
	var s = '';
	for (var i=0; i < extint_names.length; i++) {
		if (!dbgUs && extint_names[i] == 's4285') continue;	// FIXME
		s += '<option value="'+ (i+1) +'">'+ extint_names[i] +'</option>';
	}
	//console.log('extint_select_menu = '+ s);
	return s;
}
