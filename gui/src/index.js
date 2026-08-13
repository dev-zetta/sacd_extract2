'use strict';

const {
  app,
  BrowserWindow,
  dialog,
  ipcMain,
  Menu,
} = require('electron');

const {
  spawn,
} = require('node:child_process');

const fs = require('node:fs');
const path = require('node:path');

const {
  buildExtractorArguments,
  classifyExtractionResult,
  findExtractor,
  parseExtractorVersion,
} = require('./extractor');


let mainWindow = null;
let extractionProcess = null;
let outputWatcher = null;
let outputScanTimer = null;
let cancellationRequested = false;
let extractorVersion = null;
let extractorVersionPromise = null;

let detectedTrackFiles = new Set();
let initialAudioFiles = new Set();
let initialAudioFileStats = new Map();
let trackCounter = 0;


function getPreferencesPath() {
  return path.join(
    app.getPath('userData'),
    'preferences.json'
  );
}


function readLastIsoDirectory() {
  try {
    const preferences = JSON.parse(
      fs.readFileSync(
        getPreferencesPath(),
        'utf8'
      )
    );

    if (
      preferences.lastIsoDirectory &&
      fs.existsSync(preferences.lastIsoDirectory)
    ) {
      return preferences.lastIsoDirectory;
    }
  } catch {
    // Missing or invalid preferences are treated as a first launch.
  }

  return null;
}


function saveLastIsoDirectory(directory) {
  try {
    const preferencesPath =
      getPreferencesPath();

    fs.mkdirSync(
      path.dirname(preferencesPath),
      { recursive: true }
    );

    fs.writeFileSync(
      preferencesPath,
      JSON.stringify(
        { lastIsoDirectory: directory },
        null,
        2
      ),
      'utf8'
    );
  } catch (error) {
    console.warn(
      'Unable to save last ISO directory:',
      error.message
    );
  }
}


/*
 * Send an IPC message only if the renderer window still exists.
 */
function sendToRenderer(channel, payload) {
  if (
    !mainWindow ||
    mainWindow.isDestroyed()
  ) {
    return;
  }

  mainWindow.webContents.send(
    channel,
    payload
  );
}


/*
 * Send extractor output to both:
 *
 * 1. the terminal where npm start was launched;
 * 2. the Extraction log in the renderer.
 */
function sendExtractorOutput(source, data) {
  const text = Buffer.isBuffer(data)
    ? data.toString('utf8')
    : String(data ?? '');

  if (!text) {
    return;
  }

  if (source === 'stderr') {
    process.stderr.write(text);
  } else {
    process.stdout.write(text);
  }

  sendToRenderer(
    'sacd:output',
    text
  );
}


function verifyExtractorVersion() {
  if (extractorVersionPromise) {
    return extractorVersionPromise;
  }

  extractorVersionPromise = new Promise((resolve, reject) => {
    let extractorPath;

    try {
      extractorPath = getExtractorPath();
    } catch (error) {
      reject(error);
      return;
    }

    const versionProcess = spawn(
      extractorPath,
      ['-v'],
      {
        cwd: path.dirname(extractorPath),
        windowsHide: true,
        shell: false,
        stdio: ['ignore', 'pipe', 'pipe'],
      }
    );

    let output = '';

    versionProcess.stdout.on('data', (data) => {
      output += data.toString('utf8');
    });

    versionProcess.stderr.on('data', (data) => {
      output += data.toString('utf8');
    });

    versionProcess.once('error', reject);

    versionProcess.once('close', (exitCode) => {
      const version = parseExtractorVersion(output);

      if (exitCode !== 0 || !version) {
        reject(new Error(
          'sacd_extract version check failed.'
        ));
        return;
      }

      extractorVersion = version;
      resolve({
        path: extractorPath,
        version,
      });
    });
  });

  return extractorVersionPromise;
}


function installContextMenu() {
  mainWindow.webContents.on(
    'context-menu',
    (_event, params) => {
      const menuItems = [
        {
          role: 'copy',
          enabled: Boolean(params.selectionText),
        },
        {
          role: 'selectAll',
        },
      ];

      if (params.isEditable) {
        menuItems.push({
          role: 'paste',
        });
      }

      Menu.buildFromTemplate(menuItems).popup({
        window: mainWindow,
      });
    }
  );
}


/*
 * Create the main application window.
 */
function createWindow() {
  mainWindow = new BrowserWindow({
    width: 1000,
    height: 760,

    minWidth: 850,
    minHeight: 650,

    backgroundColor: '#101419',

    webPreferences: {
      preload: path.join(
        __dirname,
        'preload.js'
      ),

      contextIsolation: true,
      nodeIntegration: false,
    },
  });

  mainWindow.loadFile(
    path.join(
      __dirname,
      'index.html'
    )
  );

  installContextMenu();

  mainWindow.webContents.once(
    'did-finish-load',
    async () => {
      try {
        const result = await verifyExtractorVersion();

        sendToRenderer(
          'sacd:status',
          {
            state: 'engine-ready',
            version: result.version,
          }
        );
      } catch (error) {
        sendToRenderer(
          'sacd:status',
          {
            state: 'engine-error',
            message: error.message,
          }
        );
      }
    }
  );

  mainWindow.on('closed', () => {
    mainWindow = null;
  });

  // Uncomment temporarily when renderer debugging is needed.
  // mainWindow.webContents.openDevTools();
}


function getExtractorPath() {
  return findExtractor({
    resourcesPath: process.resourcesPath,
    appDirectory: __dirname,
  });
}


/*
 * Return true for extracted audio files.
 */
function isExtractedAudioFile(filename) {
  const extension =
    path.extname(filename).toLowerCase();

  return (
    extension === '.dsf' ||
    extension === '.dff'
  );
}


/*
 * Recursively enumerate existing DSF/DFF files.
 *
 * This prevents old files already present in the destination from
 * being counted as tracks created by the current extraction.
 */
function collectAudioFiles(directory) {
  const result = new Set();

  function scan(currentDirectory) {
    let entries;

    try {
      entries = fs.readdirSync(
        currentDirectory,
        {
          withFileTypes: true,
        }
      );
    } catch (error) {
      console.warn(
        `Unable to scan directory "${currentDirectory}":`,
        error.message
      );

      return;
    }

    for (const entry of entries) {
      const fullPath = path.join(
        currentDirectory,
        entry.name
      );

      if (entry.isDirectory()) {
        scan(fullPath);
        continue;
      }

      if (
        entry.isFile() &&
        isExtractedAudioFile(entry.name)
      ) {
        result.add(
          path.normalize(fullPath)
        );
      }
    }
  }

  scan(directory);

  return result;
}


/*
 * Ask before removing audio files that the extractor may overwrite.
 */
async function confirmAndRemoveExistingAudioFiles(outputDirectory) {
  const existingFiles =
    collectAudioFiles(outputDirectory);

  if (existingFiles.size === 0) {
    return {
      confirmed: true,
      removed: 0,
    };
  }

  const result =
    await dialog.showMessageBox(mainWindow, {
      type: 'warning',
      title: 'Audio files already exist',
      message:
        `${existingFiles.size} DSF/DFF files already exist ` +
        'in the destination.',
      detail:
        'Remove the existing audio files and create new ones?\n\n' +
        outputDirectory,
      buttons: [
        'Overwrite',
        'Cancel',
      ],
      defaultId: 1,
      cancelId: 1,
      noLink: true,
    });

  if (result.response !== 0) {
    return {
      confirmed: false,
      removed: 0,
    };
  }

  let removed = 0;

  for (const filePath of existingFiles) {
    fs.rmSync(filePath, {
      force: true,
    });

    removed += 1;
  }

  return {
    confirmed: true,
    removed,
  };
}


/*
 * Stop watching the output folder.
 */
function stopOutputWatcher() {
  if (outputWatcher) {
    try {
      outputWatcher.close();
    } catch (error) {
      console.warn(
        'Unable to close output watcher:',
        error.message
      );
    }

    outputWatcher = null;
  }

  if (outputScanTimer) {
    clearInterval(outputScanTimer);
    outputScanTimer = null;
  }
}


/*
 * Report a newly created DSF/DFF file as a track.
 *
 * sacd_extract builds do not always write progress information
 * to redirected stdout/stderr. Watching the destination directory gives
 * us a reliable fallback.
 */
function reportDetectedTrack(fullPath) {
  const normalizedPath =
    path.normalize(fullPath);

  if (detectedTrackFiles.has(normalizedPath)) {
    return;
  }

  let stats;

  try {
    stats = fs.statSync(normalizedPath);
  } catch {
    /*
     * A filesystem notification may arrive before the file is fully
     * visible. Another notification normally follows.
     */
    return;
  }

  if (!stats.isFile()) {
    return;
  }

  const initialStats =
    initialAudioFileStats.get(normalizedPath);

  if (
    initialStats &&
    initialStats.size === stats.size &&
    initialStats.mtimeMs === stats.mtimeMs
  ) {
    return;
  }

  detectedTrackFiles.add(normalizedPath);
  trackCounter += 1;

  const filename =
    path.basename(normalizedPath);

  const message =
    `\nTrack ${trackCounter}: ${filename}\n`;

  console.log(
    `[watcher] ${message.trim()}`
  );

  sendToRenderer(
    'sacd:output',
    message
  );

  /*
   * Send a dedicated event as well. The renderer may use it directly
   * later instead of parsing the log.
   */
  sendToRenderer(
    'sacd:track',
    {
      number: trackCounter,
      filename,
      path: normalizedPath,
    }
  );
}


/*
 * Polling complements fs.watch because some extractor builds create files
 * in nested directories without producing a usable recursive event.
 */
function scanOutputForTracks(outputDirectory) {
  const currentAudioFiles =
    collectAudioFiles(outputDirectory);

  for (const fullPath of currentAudioFiles) {
    reportDetectedTrack(fullPath);
  }
}


/*
 * Start watching the output directory recursively.
 *
 * Polling remains active as a portable fallback when recursive fs.watch is
 * unavailable or incomplete on the host platform.
 */
function startOutputWatcher(outputDirectory) {
  stopOutputWatcher();

  initialAudioFiles =
    collectAudioFiles(outputDirectory);

  initialAudioFileStats = new Map();

  for (const fullPath of initialAudioFiles) {
    try {
      const stats = fs.statSync(fullPath);

      initialAudioFileStats.set(
        fullPath,
        {
          size: stats.size,
          mtimeMs: stats.mtimeMs,
        }
      );
    } catch {
      // The file may disappear between the directory scan and stat().
    }
  }

  detectedTrackFiles = new Set();
  trackCounter = 0;

  try {
    outputWatcher = fs.watch(
      outputDirectory,
      {
        recursive: true,
      },

      (_eventType, filename) => {
        if (!filename) {
          return;
        }

        const relativeName =
          filename.toString();

        if (!isExtractedAudioFile(relativeName)) {
          return;
        }

        const fullPath = path.join(
          outputDirectory,
          relativeName
        );

        /*
         * Delay slightly because sacd_extract may have created the
         * directory entry but not yet opened or populated the file.
         */
        setTimeout(
          () => {
            reportDetectedTrack(fullPath);
          },
          150
        );
      }
    );

    outputWatcher.on(
      'error',
      (error) => {
        console.warn(
          'Output watcher error:',
          error.message
        );

        sendToRenderer(
          'sacd:output',
          `\n[watcher] ${error.message}\n`
        );
      }
    );

    console.log(
      'Watching output directory:',
      outputDirectory
    );

  } catch (error) {
    console.warn(
      'Unable to watch output directory:',
      error.message
    );

    sendToRenderer(
      'sacd:output',
      `\n[watcher unavailable] ${error.message}\n`
    );
  }

  scanOutputForTracks(outputDirectory);

  outputScanTimer = setInterval(
    () => {
      scanOutputForTracks(outputDirectory);
    },
    500
  );
}


/*
 * Clean up state after the process has exited.
 */
function clearExtractionState() {
  stopOutputWatcher();

  extractionProcess = null;
  cancellationRequested = false;

  detectedTrackFiles = new Set();
  initialAudioFiles = new Set();
  trackCounter = 0;
}


/*
 * ISO file selection.
 */
ipcMain.handle(
  'dialog:select-iso',

  async () => {
    const lastIsoDirectory =
      readLastIsoDirectory();

    const result =
      await dialog.showOpenDialog({
        title: 'Select SACD ISO',

        defaultPath: lastIsoDirectory || undefined,

        properties: [
          'openFile',
        ],

        filters: [
          {
            name: 'SACD ISO images',
            extensions: ['iso'],
          },

          {
            name: 'All files',
            extensions: ['*'],
          },
        ],
      });

    if (
      result.canceled ||
      result.filePaths.length === 0
    ) {
      return null;
    }

    const selectedPath = result.filePaths[0];

    saveLastIsoDirectory(
      path.dirname(selectedPath)
    );

    return selectedPath;
  }
);


/*
 * Output directory selection.
 */
ipcMain.handle(
  'dialog:select-output',

  async () => {
    const result =
      await dialog.showOpenDialog({
        title: 'Select output directory',

        properties: [
          'openDirectory',
          'createDirectory',
        ],
      });

    if (
      result.canceled ||
      result.filePaths.length === 0
    ) {
      return null;
    }

    return result.filePaths[0];
  }
);


/*
 * Start extraction.
 */
ipcMain.handle(
  'sacd:start-extraction',

  async (_event, options) => {
    if (extractionProcess) {
      return {
        success: false,
        error:
          'An extraction is already running.',
      };
    }

    try {
      if (!options?.source) {
        throw new Error(
          'No source ISO was selected.'
        );
      }

      if (!options?.output) {
        throw new Error(
          'No destination directory was selected.'
        );
      }

      if (!fs.existsSync(options.source)) {
        throw new Error(
          'The source ISO does not exist:\n' +
          options.source
        );
      }

      if (!fs.existsSync(options.output)) {
        throw new Error(
          'The destination directory does not exist:\n' +
          options.output
        );
      }

      const sourceStats =
        fs.statSync(options.source);

      if (!sourceStats.isFile()) {
        throw new Error(
          'The selected source is not a file:\n' +
          options.source
        );
      }

      const outputStats =
        fs.statSync(options.output);

      if (!outputStats.isDirectory()) {
        throw new Error(
          'The selected destination is not a directory:\n' +
          options.output
        );
      }

      const overwriteResult =
        await confirmAndRemoveExistingAudioFiles(
          options.output
        );

      if (!overwriteResult.confirmed) {
        return {
          success: false,
          cancelled: true,
          error: 'Extraction was cancelled.',
        };
      }

      const extractorCheck =
        await verifyExtractorVersion();

      const extractorPath =
        extractorCheck.path;

      const args =
        buildExtractorArguments(options);

      console.log(
        'Executable:',
        extractorPath
      );

      console.log(
        'Arguments:',
        args
      );

      cancellationRequested = false;

      /*
       * Start watcher before spawning the extractor so that very early
       * output files cannot be missed.
       */
      startOutputWatcher(
        options.output
      );

      sendToRenderer(
        'sacd:status',
        {
          state: 'running',
          message: 'Extraction started',
        }
      );

      const displayedCommand = [
        `"${extractorPath}"`,

        ...args.map((argument) => {
          const value = String(argument);

          return value.includes(' ')
            ? `"${value}"`
            : value;
        }),
      ].join(' ');

      sendToRenderer(
        'sacd:output',
        `Starting:\n${displayedCommand}\n\n`
      );

      extractionProcess = spawn(
        extractorPath,
        args,
        {
          /*
           * Use the executable directory as cwd. Some Windows binaries
           * expect DLLs or auxiliary files beside the executable.
           */
          cwd: path.dirname(
            extractorPath
          ),

          windowsHide: true,
          shell: false,

          /*
           * Explicitly create pipes for stdout and stderr.
           */
          stdio: [
            'ignore',
            'pipe',
            'pipe',
          ],
        }
      );

      extractionProcess.stdout.on(
        'data',

        (data) => {
          sendExtractorOutput(
            'stdout',
            data
          );
        }
      );

      extractionProcess.stderr.on(
        'data',

        (data) => {
          sendExtractorOutput(
            'stderr',
            data
          );
        }
      );

      extractionProcess.on(
        'error',

        (error) => {
          console.error(
            'Unable to start sacd_extract:',
            error
          );

          sendToRenderer(
            'sacd:output',
            `\nError: ${error.message}\n`
          );

          sendToRenderer(
            'sacd:status',
            {
              state: 'error',
              message: error.message,
            }
          );

          clearExtractionState();
        }
      );

      extractionProcess.on(
        'close',

        (exitCode, signal) => {
          console.log(
            'sacd_extract finished:',
            {
              exitCode,
              signal,
              cancellationRequested,
            }
          );

          /*
           * Keep a local copy because clearExtractionState resets it.
           */
          const wasCancelled =
            cancellationRequested;

          const result = classifyExtractionResult(
            exitCode,
            wasCancelled
          );

          sendToRenderer(
            'sacd:status',
            {
              ...result,
              exitCode,
              signal,
            }
          );

          sendToRenderer(
            'sacd:output',
            '\nProcess finished with exit code ' +
            `${exitCode}.\n`
          );

          clearExtractionState();
        }
      );

      return {
        success: true,
        executable: extractorPath,
        arguments: args,
      };
    } catch (error) {
      console.error(error);

      clearExtractionState();

      return {
        success: false,
        error:
          error instanceof Error
            ? error.message
            : String(error),
      };
    }
  }
);


/*
 * Stop extraction.
 */
ipcMain.handle(
  'sacd:stop-extraction',

  async () => {
    if (!extractionProcess) {
      return {
        success: false,
        error:
          'No extraction is running.',
      };
    }

    cancellationRequested = true;

    let stopped = false;

    try {
      stopped =
        extractionProcess.kill();
    } catch (error) {
      return {
        success: false,
        error:
          error instanceof Error
            ? error.message
            : String(error),
      };
    }

    return {
      success: stopped,

      error: stopped
        ? undefined
        : 'The operating system did not terminate the process.',
    };
  }
);


/*
 * Electron lifecycle.
 */
app.whenReady().then(() => {
  createWindow();

  app.on('activate', () => {
    if (
      BrowserWindow.getAllWindows()
        .length === 0
    ) {
      createWindow();
    }
  });
});


app.on(
  'before-quit',

  () => {
    stopOutputWatcher();

    if (extractionProcess) {
      cancellationRequested = true;

      try {
        extractionProcess.kill();
      } catch {
        // Process may already have exited.
      }
    }
  }
);


app.on(
  'window-all-closed',

  () => {
    if (process.platform !== 'darwin') {
      app.quit();
    }
  }
);
