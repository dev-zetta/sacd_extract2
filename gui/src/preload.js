'use strict';

const {
  contextBridge,
  ipcRenderer,
} = require('electron');


contextBridge.exposeInMainWorld('sacd', {
  selectIso: () => {
    return ipcRenderer.invoke(
      'dialog:select-iso'
    );
  },

  selectOutput: () => {
    return ipcRenderer.invoke(
      'dialog:select-output'
    );
  },

  startExtraction: (options) => {
    return ipcRenderer.invoke(
      'sacd:start-extraction',
      options
    );
  },

  stopExtraction: () => {
    return ipcRenderer.invoke(
      'sacd:stop-extraction'
    );
  },

  onOutput: (callback) => {
    const listener = (
      _event,
      payload
    ) => {
      callback(payload);
    };

    ipcRenderer.on(
      'sacd:output',
      listener
    );

    return () => {
      ipcRenderer.removeListener(
        'sacd:output',
        listener
      );
    };
  },

  onStatus: (callback) => {
    const listener = (
      _event,
      payload
    ) => {
      callback(payload);
    };

    ipcRenderer.on(
      'sacd:status',
      listener
    );

    return () => {
      ipcRenderer.removeListener(
        'sacd:status',
        listener
      );
    };
  },

  onTrack: (callback) => {
    const listener = (
      _event,
      payload
    ) => {
      callback(payload);
    };

    ipcRenderer.on(
      'sacd:track',
      listener
    );

    return () => {
      ipcRenderer.removeListener(
        'sacd:track',
        listener
      );
    };
  },
});