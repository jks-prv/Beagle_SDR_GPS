function admin_interface()
{
	visible_block('id-kiwi-container-1', 1);
	
	var admin = html("id-admin");
	admin.innerHTML =
		'<div><strong> Admin interface </strong><span id="id-problems"></span></div><br/>' +
		'<span id="id-msg-config"></span><br/>' +
		'<span id="id-msg-gps"></span><br/>' +
		'<div id="id-info-1"></div>' +
		'<div id="id-info-2"></div>' +
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
	
	ws_admin = open_websocket("ADM", timestamp);
	setTimeout(function() { setInterval(function() { ws_admin.send("SET keepalive") }, 5000) }, 5000);
	setTimeout(function() { setInterval(update_TOD, 1000); }, 1000);
}
