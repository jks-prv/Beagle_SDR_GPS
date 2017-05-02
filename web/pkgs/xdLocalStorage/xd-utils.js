/**
 * Created by Ofir_Dagan on 4/17/14.
 */
'use strict';
window.XdUtils = window.XdUtils || (function () {

  function extend(object, defaultObject) {
    var result = defaultObject || {};
    var key;
    for (key in object) {
      if (object.hasOwnProperty(key)) {
        result[key] = object[key];
      }
    }
    return result;
  }

  //public interface
  return {
    extend : extend
  };
})();
