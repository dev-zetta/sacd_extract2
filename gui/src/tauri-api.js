'use strict';


const tauri = window.__TAURI__;


function subscribe(eventName, callback) {
  let disposed = false;
  let removeListener = null;

  tauri.event.listen(eventName, (event) => {
    callback(event.payload);
  }).then((unlisten) => {
    if (disposed) {
      unlisten();
    } else {
      removeListener = unlisten;
    }
  }).catch((error) => {
    console.error(`Unable to listen for ${eventName}:`, error);
  });

  return () => {
    disposed = true;

    if (removeListener) {
      removeListener();
      removeListener = null;
    }
  };
}


if (tauri?.core && tauri?.event) {
  window.sacd = {
    selectIso: () => tauri.core.invoke('select_iso'),
    selectOutput: () => tauri.core.invoke('select_output'),
    checkEngine: () => tauri.core.invoke('check_engine'),
    startExtraction: (options) => tauri.core.invoke(
      'start_extraction',
      { options }
    ),
    stopExtraction: () => tauri.core.invoke('stop_extraction'),
    onOutput: (callback) => subscribe('sacd-output', callback),
    onStatus: (callback) => subscribe('sacd-status', callback),
    onTrack: (callback) => subscribe('sacd-track', callback),
  };
}
