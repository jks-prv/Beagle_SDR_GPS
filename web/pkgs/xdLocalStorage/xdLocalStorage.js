/**
 * Created by dagan on 07/04/2014.
 */
'use strict';
/* global console, XdUtils */
window.xdLocalStorage = window.xdLocalStorage || (function () {
  var MESSAGE_NAMESPACE = 'cross-domain-local-message';
  var options = {
    iframeId: 'cross-domain-iframe',
    iframeUrl: undefined,
    initCallback: function () {}
  };
  var requestId = -1;
  var iframe;
  var requests = {};
  var wasInit = false;
  var iframeReady = false;

  function applyCallback(data) {
    if (requests[data.id]) {
      requests[data.id](data);
      delete requests[data.id];
    }
  }

  function receiveMessage(event) {
    var data;
    try {
      data = JSON.parse(event.data);
    } catch (err) {
      //not our message, can ignore
    }
    if (data && data.namespace === MESSAGE_NAMESPACE) {
      if (data.id === 'iframe-ready') {
        iframeReady = true;
        if (data.ip) console.log('xdLocalStorage ip='+ data.ip);
        options.initCallback();
      } else {
        applyCallback(data);
      }
    }
  }

  function buildMessage(action, key, value, callback) {
    requestId++;
    requests[requestId] = callback;
    var data = {
      namespace: MESSAGE_NAMESPACE,
      etag: 0,
      server: 0,
      id: requestId,
      action: action,
      key: key,
      value: value
    };
    iframe.contentWindow.postMessage(JSON.stringify(data), '*');
  }

  function init(customOptions) {
    options = XdUtils.extend(customOptions, options);
    var temp = document.createElement('div');

    if (window.addEventListener) {
      window.addEventListener('message', receiveMessage, false);
    } else {
      window.attachEvent('onmessage', receiveMessage);
    }

    temp.innerHTML = '<iframe id="' + options.iframeId + '" src=' + options.iframeUrl + ' style="display: none;"></iframe>';
    document.body.appendChild(temp);
    iframe = document.getElementById(options.iframeId);
  }

  function isApiReady() {
    if (!wasInit) {
      console.log('You must call xdLocalStorage.init() before using it.');
      return false;
    }
    if (!iframeReady) {
      console.log('You must wait for iframe ready message before using the api.');
      return false;
    }
    return true;
  }

  return {
    //callback is optional for cases you use the api before window load.
    init: function (customOptions) {
      if (!customOptions.iframeUrl) {
        throw 'You must specify iframeUrl';
      }
      if (wasInit) {
        console.log('xdLocalStorage was already initialized!');
        return;
      }
      wasInit = true;
      if (document.readyState === 'complete') {
        init(customOptions);
      } else {
        window.onload = function () {
          init(customOptions);
        };
      }
    },
    
    setItem: function (key, value, callback) {
      if (!isApiReady()) {
        return;
      }
      buildMessage('set', key, value, callback);
    },

    getItem: function (key, callback) {
      if (!isApiReady()) {
        return;
      }
      buildMessage('get', key,  null, callback);
    },
    
    removeItem: function (key, callback) {
      if (!isApiReady()) {
        return;
      }
      buildMessage('remove', key,  null, callback);
    },
    
    key: function (index, callback) {
      if (!isApiReady()) {
        return;
      }
      buildMessage('key', index,  null, callback);
    },
    
    getSize: function(callback) {
      if(!isApiReady()) {
        return;
      }
      buildMessage('size', null, null, callback);
    },
    
    getLength: function(callback) {
      if(!isApiReady()) {
        return;
      }
      buildMessage('length', null, null, callback);
    },
    
    clear: function (callback) {
      if (!isApiReady()) {
        return;
      }
      buildMessage('clear', null,  null, callback);
    },
    
    wasInit: function () {
      return wasInit;
    }
  };
})();

window.xdLocalStorageHA = window.xdLocalStorageHA || (function () {
  var MESSAGE_NAMESPACE = 'cross-domain-local-message';
  var options = {
    iframeId: 'cross-domain-iframe',
    iframeUrls: undefined,
    initCallback: function () {}
  };
  var requestId = -1;
  var iframes = [];
  var seen = [];
  var seen_ct = 0;
  var requests = {};
  var replies = [];
  var wasInit = false;
  var iframeReady = false;

	setTimeout(function() {
		buildMessage('ping', null, null, null);

		setTimeout(function() {
			console.log('XDLS seen servers='+ seen_ct);
			iframeReady = true;
			options.initCallback();
		}, 1000);
	}, 5000);

  function applyCallback(data) {
	//jksx
	console.log('XDLS result id='+ data.id +' server='+ data.server);
	console.log(data);
    if (requests[data.id]) {
      requests[data.id](data);
      delete requests[data.id];
    }
  }

  function receiveMessage(event) {
    var data;
    try {
      data = JSON.parse(event.data);
    } catch (err) {
      //not our message, can ignore
    }
    if (data && data.namespace === MESSAGE_NAMESPACE) {
      if (data.id === 'iframe-ready') {
        if (data.ip) console.log('xdLocalStorage ip='+ data.ip);
      } else
      if (data.action === 'ping') {
      	seen[data.server] = true;
      	seen_ct++;
			console.log('XDLS ping reply server='+ data.server +' seen_ct='+ seen_ct);
      } else {
      	replies[data.server] = data;
      }
    }
  }

	function buildMessage(action, key, value, callback) {
		requestId++;
		requests[requestId] = callback;
		var data = {
			namespace: MESSAGE_NAMESPACE,
			etag: Date.now(),
			id: requestId,
			action: action,
			key: key,
			value: value
		};
		
		for (var server = 0; server < iframes.length; server++) {
			if (action != 'ping' && !seen[server]) continue;
			//jksx
			console.log('XDLS request id='+ requestId +' server='+ server +' action='+ action +' etag='+ data.etag);
			data.server = server;
			replies[server] = null;
			iframes[server].contentWindow.postMessage(JSON.stringify(data), '*');
		}

		// Shouldn't take long for the replies since they are from the same browser.
		// I.e. this creates no traffic to the pubN.kiwisdr.com servers.
		// That only happens due to the <iframe src=[url]> when the iframe is created.
		if (action == 'ping') return;
		
		setTimeout(function() {
			var mostRecent_etag = -1, mostRecent_server = -1;

			for (var server = 0; server < iframes.length; server++) {
				var data = replies[server];
				if (data == null) continue;
				var etag = data.etag? data.etag : 0;
				//jksx
				console.log('XDLS response id='+ data.id +' server='+ data.server +' etag='+ etag);

				if (etag > mostRecent_etag) {
					mostRecent_etag = etag;
					mostRecent_server = server;
				}
        	}

			if (mostRecent_server != -1)
				applyCallback(replies[mostRecent_server]);
			else
				console.log('XDLS wait longer for response?');
		}, 250);
	}

  function init(customOptions) {
    options = XdUtils.extend(customOptions, options);

    if (window.addEventListener) {
      window.addEventListener('message', receiveMessage, false);
    } else {
      window.attachEvent('onmessage', receiveMessage);
    }

	var i = 0;
	options.iframeUrls.forEach(function(url) {
		//console.log('XDLS '+ i +' '+ url);
		var iframeId = 'id-'+ options.iframeId +'-'+ i;
		var el = document.createElement('div');
		w3_add(el, 'id-'+ options.iframeId);   // for benefit of browser inspector
		el.innerHTML = '<iframe id="' + iframeId + '" src=' + url + ' style="display: none;"></iframe>';
		document.body.appendChild(el);
		iframes[i] = document.getElementById(iframeId);
		i++;
	});
	}

  function isApiReady() {
    if (!wasInit) {
      console.log('You must call xdLocalStorage.init() before using it.');
      return false;
    }
    if (!iframeReady) {
      console.log('You must wait for iframe ready message before using the api.');
      return false;
    }
    return true;
  }

  return {
    //callback is optional for cases you use the api before window load.
    init: function (customOptions) {
      if (!customOptions.iframeUrls) {
        throw 'You must specify iframeUrls';
      }
      if (wasInit) {
        console.log('xdLocalStorage was already initialized!');
        return;
      }
      wasInit = true;
      if (document.readyState === 'complete') {
        init(customOptions);
      } else {
        window.onload = function () {
          init(customOptions);
        };
      }
    },
    
    setItem: function (key, value, callback) {
      if (!isApiReady()) {
        return;
      }
      buildMessage('set', key, value, callback);
    },

    getItem: function (key, callback) {
      if (!isApiReady()) {
        return;
      }
      buildMessage('get', key,  null, callback);
    },
    
    removeItem: function (key, callback) {
      if (!isApiReady()) {
        return;
      }
      buildMessage('remove', key,  null, callback);
    },
    
    key: function (index, callback) {
      if (!isApiReady()) {
        return;
      }
      buildMessage('key', index,  null, callback);
    },
    
    getSize: function(callback) {
      if(!isApiReady()) {
        return;
      }
      buildMessage('size', null, null, callback);
    },
    
    getLength: function(callback) {
      if(!isApiReady()) {
        return;
      }
      buildMessage('length', null, null, callback);
    },
    
    clear: function (callback) {
      if (!isApiReady()) {
        return;
      }
      buildMessage('clear', null,  null, callback);
    },
    
    wasInit: function () {
      return wasInit;
    }
  };
})();
