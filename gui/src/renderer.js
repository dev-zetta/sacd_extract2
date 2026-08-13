'use strict';


const $$ = (selector) => [
  ...document.querySelectorAll(selector),
];


const sourcePath = document.getElementById('sourcePath');
const outputPath = document.getElementById('outputPath');

const browseSourceButton =
  document.getElementById('browseSource');

const browseOutputButton =
  document.getElementById('browseOutput');

const extractButton =
  document.getElementById('extractButton');

const startIcon =
  document.getElementById('startIcon');

const waitIcon =
  document.getElementById('waitIcon');

const stopIcon =
  document.getElementById('stopIcon');

const primaryStatus =
  document.getElementById('primaryStatus');

const secondaryStatus =
  document.getElementById('secondaryStatus');

const engineLed =
  document.getElementById('engineLed');

const engineStatus =
  document.getElementById('engineStatus');

//const dropZone =
//  document.getElementById('dropZone');

const progressText =
  document.getElementById('progressText');

const progressPercent =
  document.getElementById('progressPercent');

const progressBar =
  document.getElementById('progressBar');

const trackStatus =
  document.getElementById('trackStatus');

const elapsedStatus =
  document.getElementById('elapsedStatus');

const outputStatus =
  document.getElementById('outputStatus');

const logPanel =
  document.getElementById('logPanel');

const logOutput =
  document.getElementById('logOutput');


let extractionRunning = false;
let extractorReady = false;
let extractorVersionText = '';
let extractionStartedAt = null;
let elapsedTimer = null;
let logBuffer = '';


function hasApplicationApi() {
  return Boolean(
    window.sacd &&
    typeof window.sacd.selectIso === 'function' &&
    typeof window.sacd.selectOutput === 'function' &&
    typeof window.sacd.startExtraction === 'function'
  );
}


function setEngineState(state, message) {
  engineLed.classList.remove(
    'on',
    'running',
    'error',
    'off'
  );

  switch (state) {
    case 'running':
      engineLed.classList.add('running');
      break;

    case 'error':
      engineLed.classList.add('error');
      break;

    case 'off':
      engineLed.classList.add('off');
      break;

    case 'ready':
    default:
      engineLed.classList.add('on');
      break;
  }

  engineStatus.textContent = message;
}


function formatExtractorVersion(version) {
  const value = String(version || '');
  const match = value.match(/^(.*-\d+)-g([0-9a-f]+)$/i);

  if (!match) {
    return value;
  }

  return `${match[1]}-...${match[2].slice(-6)}`;
}


function updateStartAvailability() {
  if (extractionRunning) {
    setExtractionButtonState('processing');
    extractButton.disabled = false;
    return;
  }

  const sourceSelected =
    sourcePath.value.trim().length > 0;

  const outputSelected =
    outputPath.value.trim().length > 0;

  const hasSelection =
    sourceSelected && outputSelected;

  extractButton.disabled =
    !(hasSelection && extractorReady);

  setExtractionButtonState(
    hasSelection
      ? 'ready'
      : 'waiting'
  );

  if (!sourceSelected) {
    primaryStatus.textContent =
      'Wait for ISO';

    secondaryStatus.textContent =
      'Select the source ISO and destination folder.';

    return;
  }

  if (!outputSelected) {
    primaryStatus.textContent =
      'Select a destination folder';

    secondaryStatus.textContent =
      'Choose where the extracted files should be saved.';

    return;
  }

  primaryStatus.textContent =
    'Ready';

  secondaryStatus.textContent =
    'Choose the extraction options and press Start.';
}


function setSource(filePath) {
  sourcePath.value = filePath || '';

  if (
    filePath &&
    !outputPath.value
  ) {
    const separator = filePath.includes('\\')
      ? '\\'
      : '/';

    const parts = filePath.split(separator);
    parts.pop();

    outputPath.value = parts.join(separator);
  }

  updateStartAvailability();
}


function setOutput(filePath) {
  outputPath.value = filePath || '';
  outputStatus.textContent = filePath || '—';

  updateStartAvailability();
}


function setExtractionButtonState(state) {
  extractButton.classList.remove(
    'state-waiting',
    'state-ready',
    'state-processing'
  );

  extractButton.classList.add(
    `state-${state}`
  );

  if (waitIcon) {
    waitIcon.hidden = state !== 'waiting';
  }

  if (startIcon) {
    startIcon.hidden = state !== 'ready';
  }

  if (stopIcon) {
    stopIcon.hidden = state !== 'processing';
  }

  const labels = {
    waiting: 'Waiting for ISO',
    ready: 'Ready to go',
    processing: 'Processing',
  };

  extractButton.setAttribute(
    'aria-label',
    labels[state]
  );

  extractButton.title = labels[state];
}


function setRunningUi(running) {
  extractionRunning = running;

  browseSourceButton.disabled = running;
  browseOutputButton.disabled = running;

  $$('.segment input').forEach((input) => {
    input.disabled = running;
  });

  document.getElementById('decodeDst').disabled =
    running;

  document.getElementById('exportCuesheet').disabled =
    running;

  setExtractionButtonState(
    running
      ? 'processing'
      : 'waiting'
  );

  extractButton.disabled =
    running
      ? false
      : !(
          sourcePath.value.trim() &&
          outputPath.value.trim()
        );

}


function formatElapsed(milliseconds) {
  const totalSeconds =
    Math.max(0, Math.floor(milliseconds / 1000));

  const hours =
    Math.floor(totalSeconds / 3600);

  const minutes =
    Math.floor((totalSeconds % 3600) / 60);

  const seconds =
    totalSeconds % 60;

  if (hours > 0) {
    return [
      hours,
      String(minutes).padStart(2, '0'),
      String(seconds).padStart(2, '0'),
    ].join(':');
  }

  return [
    minutes,
    String(seconds).padStart(2, '0'),
  ].join(':');
}


function startElapsedTimer() {
  stopElapsedTimer();

  extractionStartedAt = Date.now();
  elapsedStatus.textContent = '0:00';

  elapsedTimer = window.setInterval(() => {
    elapsedStatus.textContent = formatElapsed(
      Date.now() - extractionStartedAt
    );
  }, 1000);
}


function stopElapsedTimer() {
  if (elapsedTimer !== null) {
    window.clearInterval(elapsedTimer);
    elapsedTimer = null;
  }
}


function resetProgress() {
  progressText.textContent = 'Starting…';
  progressPercent.textContent = '0%';
  progressBar.style.width = '0%';

  trackStatus.textContent = '—';
  elapsedStatus.textContent = '0:00';

  outputStatus.textContent =
    outputPath.value || '—';
}


function setProgress(percent, message = null) {
  const normalized = Math.max(
    0,
    Math.min(100, Math.round(percent))
  );

  progressPercent.textContent =
    `${normalized}%`;

  progressBar.style.width =
    `${normalized}%`;

  if (message) {
    progressText.textContent = message;
  }
}


function parseExtractorOutput(text) {
  const output = String(text || '');

  const percentMatches = [
    ...output.matchAll(/(?:progress|completed|complete)\D{0,12}(\d{1,3})\s*%/gi),
    ...output.matchAll(/\b(\d{1,3})\s*%/g),
  ];

  const latestPercent = percentMatches
    .map((match) => Number(match[1]))
    .filter((percent) => percent >= 0 && percent <= 100)
    .at(-1);

  if (latestPercent !== undefined) {
    setProgress(latestPercent, 'Extracting…');
  }

  const trackMatches = [
    ...output.matchAll(/\btrack\s*[:#]?\s*(\d+)\b/gi),
  ];

  const latestTrack = trackMatches.at(-1)?.[1];

  if (latestTrack) {
    trackStatus.textContent = latestTrack;
  }
}


function appendLog(text) {
  if (!text) {
    return;
  }

  if (
    logOutput.textContent.startsWith(
      '[ready] Waiting'
    )
  ) {
    logOutput.textContent = '';
  }

  logOutput.textContent += text;
  logOutput.scrollTop = logOutput.scrollHeight;

  logBuffer += text;

  if (logBuffer.length > 20000) {
    logBuffer = logBuffer.slice(-20000);
  }

  parseExtractorOutput(logBuffer);
}


async function selectIso() {
  if (!hasApplicationApi()) {
    showFatalApiError();
    return;
  }

  try {
    const selected =
      await window.sacd.selectIso();

    if (selected) {
      setSource(selected);
    }
  } catch (error) {
    showError(
      'Unable to open the ISO selection dialog.',
      error
    );
  }
}


async function selectOutput() {
  if (!hasApplicationApi()) {
    showFatalApiError();
    return;
  }

  try {
    const selected =
      await window.sacd.selectOutput();

    if (selected) {
      setOutput(selected);
    }
  } catch (error) {
    showError(
      'Unable to open the destination dialog.',
      error
    );
  }
}


function getExtractionOptions() {
  const selectedFormat =
    document.querySelector(
      'input[name="format"]:checked'
    );

  const selectedArea =
    document.querySelector(
      'input[name="area"]:checked'
    );

  return {
    source: sourcePath.value.trim(),
    output: outputPath.value.trim(),

    format:
      selectedFormat?.value || 'dsf',

    area:
      selectedArea?.value || 'stereo',

    decodeDst:
      document.getElementById('decodeDst').checked,

    exportCuesheet:
      document.getElementById(
        'exportCuesheet'
      ).checked,
  };
}


async function startExtraction() {
  const options = getExtractionOptions();

  if (!options.source) {
    primaryStatus.textContent =
      'Select an ISO file';

    secondaryStatus.textContent =
      'A source SACD image is required.';

    return;
  }

  if (!options.output) {
    primaryStatus.textContent =
      'Select a destination folder';

    secondaryStatus.textContent =
      'An output directory is required.';

    return;
  }

  if (!hasApplicationApi()) {
    showFatalApiError();
    return;
  }

  if (!extractorReady) {
    primaryStatus.textContent =
      'Wait for extractor';

    secondaryStatus.textContent =
      'The sacd_extract version check is still running.';

    return;
  }

  setRunningUi(true);
  resetProgress();
  startElapsedTimer();

  primaryStatus.textContent =
    'Starting extraction…';

  secondaryStatus.textContent =
    'Launching sacd_extract';

  setEngineState(
    'running',
        'Processing'
  );

  logBuffer = '';
  logOutput.textContent = '';

  appendLog(
    `[start] Source: ${options.source}\n` +
    `[start] Output: ${options.output}\n` +
    `[start] Format: ${options.format.toUpperCase()}\n` +
    `[start] Area: ${options.area}\n\n`
  );

  try {
    const result =
      await window.sacd.startExtraction(options);

    if (result?.cancelled) {
      finishCancelled(
        'Extraction was not started.'
      );

      return;
    }

    if (!result?.success) {
      finishWithError(
        result?.error ||
        'The extraction process could not be started.'
      );

      return;
    }

    primaryStatus.textContent =
      'Extracting…';

    secondaryStatus.textContent =
      'sacd_extract is processing the image';

    progressText.textContent =
      'Extracting…';
  } catch (error) {
    finishWithError(
      error?.message ||
      String(error)
    );
  }
}


async function stopExtraction() {
  if (
    !window.sacd ||
    typeof window.sacd.stopExtraction !== 'function'
  ) {
    finishWithError(
      'The stopExtraction API is unavailable.'
    );

    return;
  }

  extractButton.disabled = true;

  primaryStatus.textContent =
    'Stopping extraction…';

  secondaryStatus.textContent =
    'Waiting for sacd_extract to terminate';

  setEngineState(
    'running',
    'Stopping extractor'
  );

  appendLog('\n[cancel] Cancellation requested.\n');

  try {
    const result =
      await window.sacd.stopExtraction();

    if (!result?.success) {
      extractButton.disabled = false;

      showError(
        result?.error ||
        'The process could not be stopped.'
      );
    }
  } catch (error) {
    extractButton.disabled = false;

    showError(
      'Unable to stop the extraction process.',
      error
    );
  }
}


function finishSuccessfully(message) {
  stopElapsedTimer();
  setRunningUi(false);

  setProgress(100, 'Completed');

  primaryStatus.textContent =
    'Wait for ISO';

  secondaryStatus.textContent =
    message || 'The output files are ready.';

  setEngineState(
    'ready',
    extractorVersionText ||
    'Waiting for sacd_extract'
  );

  appendLog('\n[done] Extraction completed successfully.\n');
}


function finishWithError(message) {
  stopElapsedTimer();
  setRunningUi(false);

  progressText.textContent =
    'Failed';

  primaryStatus.textContent =
    'Extraction failed';

  secondaryStatus.textContent =
    message;

  setEngineState(
    'error',
    'Extractor error'
  );

  appendLog(`\n[error] ${message}\n`);

  if (logPanel) {
    logPanel.open = true;
  }
}


function finishPartially(message) {
  stopElapsedTimer();
  setRunningUi(false);

  progressText.textContent =
    'Completed with damage';

  primaryStatus.textContent =
    'Partial output';

  secondaryStatus.textContent =
    message;

  setEngineState(
    'error',
    'Review partial output files'
  );

  appendLog(`\n[warning] ${message}\n`);

  if (logPanel) {
    logPanel.open = true;
  }
}


function finishCancelled(message) {
  stopElapsedTimer();
  setRunningUi(false);

  progressText.textContent =
    'Cancelled';

  primaryStatus.textContent =
    'Wait for ISO';

  secondaryStatus.textContent =
    message || 'The extraction process was stopped.';

  setEngineState(
    'ready',
    extractorVersionText ||
    'Waiting for sacd_extract'
  );

  appendLog('\n[cancelled] Extraction stopped.\n');
}


function showError(message, error = null) {
  const detail =
    error?.message ||
    (error ? String(error) : '');

  secondaryStatus.textContent =
    detail
      ? `${message} ${detail}`
      : message;

  appendLog(
    `\n[error] ${message}` +
    (detail ? ` ${detail}` : '') +
    '\n'
  );

  if (logPanel) {
    logPanel.open = true;
  }
}


function showFatalApiError() {
  finishWithError(
    'The Tauri application API is unavailable. Restart the application.'
  );
}


function installSegmentHandlers() {
  $$('.segment input').forEach((input) => {
    input.addEventListener('change', () => {
      $$(
        `input[name="${input.name}"]`
      ).forEach((item) => {
        item
          .closest('.segment')
          .classList.toggle(
            'active',
            item.checked
          );
      });
    });
  });
}


function handleStatus(status) {
  if (!status || !status.state) {
    return;
  }

  switch (status.state) {
    case 'engine-ready':
      extractorReady = true;
      extractorVersionText =
        `sacd_extract client ` +
        `${formatExtractorVersion(status.version)} ready`;

      setEngineState(
        'ready',
        extractorVersionText
      );

      updateStartAvailability();
      break;

    case 'engine-error':
      extractorReady = false;
      extractorVersionText = '';

      setEngineState(
        'error',
        status.message || 'Extractor unavailable'
      );

      updateStartAvailability();
      break;

    case 'running':
      setRunningUi(true);

      primaryStatus.textContent =
        'Processing';

      secondaryStatus.textContent =
        status.message ||
        'sacd_extract is running';

      setEngineState(
        'running',
        'Processing'
      );

      break;

    case 'completed':
      finishSuccessfully(
        status.message
      );

      break;

    case 'partial':
      finishPartially(
        status.message ||
        'Extraction completed with damaged output files.'
      );

      break;

    case 'cancelled':
      finishCancelled(
        status.message
      );

      break;

    case 'error':
      finishWithError(
        status.message ||
        (
          status.exitCode !== undefined
            ? `Exit code ${status.exitCode}`
            : 'Unknown extraction error'
        )
      );

      break;

    default:
      console.warn(
        'Unknown extraction state:',
        status
      );
  }
}


function installApplicationListeners() {
  if (
    window.sacd &&
    typeof window.sacd.onOutput === 'function'
  ) {
    window.sacd.onOutput((text) => {
      appendLog(String(text));
    });
  }

  if (
    window.sacd &&
    typeof window.sacd.onTrack === 'function'
  ) {
    window.sacd.onTrack((track) => {
      if (
        track &&
        Number.isFinite(Number(track.number))
      ) {
        trackStatus.textContent =
          String(track.number);
      }

      if (track?.filename) {
        appendLog(
          `[track] ${track.number}: ${track.filename}\n`
        );
      }
    });
  }

  if (
    window.sacd &&
    typeof window.sacd.onStatus === 'function'
  ) {
    window.sacd.onStatus(handleStatus);
  }
}


browseSourceButton.addEventListener(
  'click',
  selectIso
);


browseOutputButton.addEventListener(
  'click',
  selectOutput
);


extractButton.addEventListener(
  'click',
  async () => {
    if (extractionRunning) {
      await stopExtraction();
    } else {
      await startExtraction();
    }
  }
);


installSegmentHandlers();
installApplicationListeners();


if (hasApplicationApi()) {
  setEngineState(
    'off',
    'Waiting for sacd_extract'
  );

  window.sacd.checkEngine()
    .then(handleStatus)
    .catch((error) => {
      handleStatus({
        state: 'engine-error',
        message: error?.message || String(error),
      });
    });
} else {
  setEngineState(
    'error',
    'Tauri integration unavailable'
  );

  primaryStatus.textContent =
    'Application integration error';

  secondaryStatus.textContent =
    'Restart the desktop application.';
}


updateStartAvailability();
