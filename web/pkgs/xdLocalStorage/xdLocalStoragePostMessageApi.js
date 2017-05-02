/**
 * Created by dagan on 07/04/2014.
 */
'use strict';
/* global XdUtils */
(function () {

  var MESSAGE_NAMESPACE = 'cross-domain-local-message';

  var replyData = {
    namespace: MESSAGE_NAMESPACE
  };

  function postData(id, data) {
    var mergedData = XdUtils.extend(data, replyData);
    mergedData.etag = localStorage.getItem('etag');
    mergedData.id = id;
    parent.postMessage(JSON.stringify(mergedData), '*');
  }

  function getData(id, key) {
    var value = localStorage.getItem(key);
    var data = {
      key: key,
      value: value
    };
    postData(id, data);
  }

  function setData(etag, id, key, value) {
    localStorage.setItem(key, value);
	 localStorage.setItem('etag', etag);
    var checkGet = localStorage.getItem(key);
    var data = {
      success: checkGet === value
    };
    postData(id, data);
  }

  function removeData(etag, id, key) {
    localStorage.removeItem(key);
	 localStorage.setItem('etag', etag);
    postData(id, {});
  }

  function getKey(id, index) {
    var key = localStorage.key(index);
    postData(id, {key: key});
  }

  function getSize(id) {
    var size = JSON.stringify(localStorage).length;
    postData(id, {size: size});
  }

  function getLength(id) {
    var length = localStorage.length;
    postData(id, {length: length});
  }

  function clear(etag, id) {
    localStorage.clear();
	 localStorage.setItem('etag', etag);
    postData(id, {});
  }

  function ping(id) {
    postData(id, {});
  }
  
  function receiveMessage(event) {
    var data;
    try {
      data = JSON.parse(event.data);
    } catch (err) {
      //not our message, can ignore
    }

    if (data && data.namespace === MESSAGE_NAMESPACE) {
    	replyData.action = data.action;
    	replyData.server = data.server;

      if (data.action === 'set') {
        setData(data.etag, data.id, data.key, data.value);
      } else if (data.action === 'get') {
        getData(data.id, data.key);
      } else if (data.action === 'remove') {
        removeData(data.etag, data.id, data.key);
      } else if (data.action === 'key') {
        getKey(data.id, data.key);
      } else if (data.action === 'size') {
        getSize(data.id);
      } else if (data.action === 'length') {
        getLength(data.id);
      } else if (data.action === 'clear') {
        clear(data.etag, data.id);
      } else if (data.action === 'ping') {
        ping(data.id);
      }
    }
  }

  if (window.addEventListener) {
    window.addEventListener('message', receiveMessage, false);
  } else {
    window.attachEvent('onmessage', receiveMessage);
  }

  function sendOnLoad() {
    var data = {
      namespace: MESSAGE_NAMESPACE,
      id: 'iframe-ready',
      ip: xdLocalStorage_ip
    };
    parent.postMessage(JSON.stringify(data), '*');
  }
  
  //on creation
  sendOnLoad();
})();
