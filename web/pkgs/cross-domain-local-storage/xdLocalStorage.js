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
  var iframeReady = true;

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
