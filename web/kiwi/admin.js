/*
admin page
	show
		x h/w serno
		x IP addresses
		x s/w version
	options
		pwds
		update
			[auto, ask me, none, force]
		report problems to kiwisdr.com
		public access
		set static ip/nm/gw? hostname?
		throttle
			#chans
			MB/day GB/mo
		enable
			GPS
	config
		Beagle static IP address
		Beagle hostname
		dx list
			rewrite config file on update
		region = 1/2/3, easier way of setting channel spacings, band plans
*/

function admin_interface()
{
	var admin = html("id-admin");
	admin.innerHTML =
		'<div><strong> Admin interface </strong>' +
			'<span id="id-problems"></span>' +
		'</div><br/>' +
		'<span id="id-msg-config"></span><br/>' +
		'<span id="id-msg-gps"></span><br/>' +
		'<div id="id-info-1"></div>' +
		'<div id="id-info-2"></div>' +
		'<br/>' +
		'<span id="id-msg-config2"></span><br/>' +
		'<br/>' +
		'<div id="id-debugdiv"></div>' +
		'';
	admin.style.top = admin.style.left = '10px';
	var i1 = html('id-info-1');
	var i2 = html('id-info-2');
	i1.style.position = i2.style.position = 'static';
	i1.style.fontSize = i2.style.fontSize = '100%';
	i1.style.color = i2.style.color = 'white';
	visible_block('id-admin', 1);
	
	ws_admin = open_websocket("ADM", timestamp, adm_recv);
	setTimeout(function() { setInterval(function() { ws_admin.send("SET keepalive") }, 5000) }, 5000);
	setTimeout(function() { setInterval(update_TOD, 1000); }, 1000);
}

function adm_recv(data)
{
	var s='';
	var stringData = arrayBufferToString(data);
	params = stringData.substring(4).split(" ");
	for (var i=0; i < params.length; i++) {
		s += ' '+params[i];
		param = params[i].split("=");
		switch (param[0]) {
			case "init":
				rx_chans = rx_chan = param[1];
				console.log("ADMIN init rx_chans="+rx_chans);
				users_init();
				break;
			default:
				s += " ???";
				break;
		}
	}
	//console.log('ADM: '+s);
}
