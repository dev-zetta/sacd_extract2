use serde::{Deserialize, Serialize};
use std::collections::HashSet;
use std::fs;
use std::path::{Path, PathBuf};
use std::sync::Mutex;
use std::time::Duration;
use tauri::{AppHandle, Emitter, Manager, RunEvent, State};
use tauri_plugin_dialog::{DialogExt, MessageDialogButtons, MessageDialogKind};
use tauri_plugin_shell::process::{CommandChild, CommandEvent};
use tauri_plugin_shell::ShellExt;

const SIDECAR_NAME: &str = "sacd_extract";

#[derive(Default)]
struct ApplicationState {
    inner: Mutex<ProcessState>,
}

#[derive(Default)]
struct ProcessState {
    next_id: u64,
    running: Option<RunningProcess>,
    cancelled_id: Option<u64>,
}

struct RunningProcess {
    id: u64,
    child: CommandChild,
}

#[derive(Clone, Debug, Deserialize)]
#[serde(rename_all = "camelCase")]
struct ExtractionOptions {
    source: String,
    output: String,
    format: String,
    area: String,
    decode_dst: bool,
    export_cuesheet: bool,
}

#[derive(Clone, Debug, Serialize)]
#[serde(rename_all = "camelCase")]
struct CommandResponse {
    success: bool,
    #[serde(skip_serializing_if = "Option::is_none")]
    cancelled: Option<bool>,
    #[serde(skip_serializing_if = "Option::is_none")]
    error: Option<String>,
    #[serde(skip_serializing_if = "Option::is_none")]
    executable: Option<String>,
    #[serde(skip_serializing_if = "Option::is_none")]
    arguments: Option<Vec<String>>,
}

impl CommandResponse {
    fn success(arguments: Vec<String>) -> Self {
        Self {
            success: true,
            cancelled: None,
            error: None,
            executable: Some(SIDECAR_NAME.to_string()),
            arguments: Some(arguments),
        }
    }

    fn stopped() -> Self {
        Self {
            success: true,
            cancelled: None,
            error: None,
            executable: None,
            arguments: None,
        }
    }

    fn error(message: impl Into<String>) -> Self {
        Self {
            success: false,
            cancelled: None,
            error: Some(message.into()),
            executable: None,
            arguments: None,
        }
    }

    fn cancelled() -> Self {
        Self {
            success: false,
            cancelled: Some(true),
            error: Some("Extraction was cancelled.".to_string()),
            executable: None,
            arguments: None,
        }
    }
}

#[derive(Clone, Debug, Serialize)]
#[serde(rename_all = "camelCase")]
struct StatusPayload {
    state: String,
    #[serde(skip_serializing_if = "Option::is_none")]
    message: Option<String>,
    #[serde(skip_serializing_if = "Option::is_none")]
    version: Option<String>,
    #[serde(skip_serializing_if = "Option::is_none")]
    exit_code: Option<i32>,
    #[serde(skip_serializing_if = "Option::is_none")]
    signal: Option<i32>,
}

#[derive(Clone, Debug, Serialize)]
struct TrackPayload {
    number: usize,
    filename: String,
    path: String,
}

#[derive(Default, Deserialize, Serialize)]
#[serde(rename_all = "camelCase")]
struct Preferences {
    last_iso_directory: Option<PathBuf>,
}

fn build_extractor_arguments(options: &ExtractionOptions) -> Vec<String> {
    let mut arguments = vec!["--input".to_string(), options.source.clone()];

    arguments.push(if options.format == "dff" {
        "--dsdiff".to_string()
    } else {
        "--dsf".to_string()
    });

    arguments.push("--track-output-dir".to_string());
    arguments.push(options.output.clone());

    match options.area.as_str() {
        "multichannel" => arguments.push("--multichannel".to_string()),
        "both" => {
            arguments.push("--stereo".to_string());
            arguments.push("--multichannel".to_string());
        }
        _ => arguments.push("--stereo".to_string()),
    }

    if options.decode_dst {
        arguments.push("--convert-dst".to_string());
    }

    if options.export_cuesheet {
        arguments.push("--cue".to_string());
    }

    arguments
}

fn parse_extractor_version(output: &str) -> Option<String> {
    let words: Vec<&str> = output.split_whitespace().collect();

    words.windows(3).find_map(|window| {
        (window[0].eq_ignore_ascii_case("sacd_extract") && window[1].eq_ignore_ascii_case("client"))
            .then(|| window[2].to_string())
    })
}

fn classify_result(exit_code: Option<i32>, cancelled: bool) -> StatusPayload {
    if cancelled || exit_code == Some(130) {
        return StatusPayload {
            state: "cancelled".to_string(),
            message: Some("Extraction was interrupted.".to_string()),
            version: None,
            exit_code,
            signal: None,
        };
    }

    let (state, message) = match exit_code {
        Some(0) => ("completed", "Extraction completed successfully."),
        Some(1) => (
            "partial",
            "Extraction completed with partial or abandoned outputs.",
        ),
        Some(code) => {
            return StatusPayload {
                state: "error".to_string(),
                message: Some(format!("Extraction failed with exit code {code}.")),
                version: None,
                exit_code,
                signal: None,
            };
        }
        None => ("error", "Extraction ended without an exit status."),
    };

    StatusPayload {
        state: state.to_string(),
        message: Some(message.to_string()),
        version: None,
        exit_code,
        signal: None,
    }
}

fn is_audio_file(path: &Path) -> bool {
    path.extension()
        .and_then(|extension| extension.to_str())
        .is_some_and(|extension| {
            extension.eq_ignore_ascii_case("dsf") || extension.eq_ignore_ascii_case("dff")
        })
}

fn collect_audio_files(directory: &Path) -> Vec<PathBuf> {
    let mut files = Vec::new();
    collect_audio_files_into(directory, &mut files);
    files.sort();
    files
}

fn collect_audio_files_into(directory: &Path, files: &mut Vec<PathBuf>) {
    let Ok(entries) = fs::read_dir(directory) else {
        return;
    };

    for entry in entries.flatten() {
        let path = entry.path();

        if path.is_dir() {
            collect_audio_files_into(&path, files);
        } else if path.is_file() && is_audio_file(&path) {
            files.push(path);
        }
    }
}

fn validate_extraction_paths(options: &ExtractionOptions) -> Result<(), String> {
    if options.source.is_empty() {
        return Err("No source ISO was selected.".to_string());
    }

    if options.output.is_empty() {
        return Err("No destination directory was selected.".to_string());
    }

    let source = Path::new(&options.source);
    let output = Path::new(&options.output);

    if !source.is_file() {
        return Err(format!(
            "The source ISO does not exist or is not a file:\n{}",
            source.display()
        ));
    }

    if !output.is_dir() {
        return Err(format!(
            "The destination does not exist or is not a directory:\n{}",
            output.display()
        ));
    }

    Ok(())
}

fn preferences_path(app: &AppHandle) -> Option<PathBuf> {
    app.path()
        .app_config_dir()
        .ok()
        .map(|directory| directory.join("preferences.json"))
}

fn read_preferences(app: &AppHandle) -> Preferences {
    preferences_path(app)
        .and_then(|path| fs::read_to_string(path).ok())
        .and_then(|text| serde_json::from_str(&text).ok())
        .unwrap_or_default()
}

fn save_preferences(app: &AppHandle, preferences: &Preferences) {
    let Some(path) = preferences_path(app) else {
        return;
    };

    let Some(parent) = path.parent() else {
        return;
    };

    if fs::create_dir_all(parent).is_err() {
        return;
    }

    if let Ok(text) = serde_json::to_string_pretty(preferences) {
        let _ = fs::write(path, text);
    }
}

fn extractor_command(app: &AppHandle) -> Result<tauri_plugin_shell::process::Command, String> {
    if let Some(path) = std::env::var_os("SACD_EXTRACT_PATH") {
        let path = PathBuf::from(path);

        if !path.is_file() {
            return Err(format!(
                "SACD_EXTRACT_PATH is not a file: {}",
                path.display()
            ));
        }

        return Ok(app.shell().command(path));
    }

    app.shell()
        .sidecar(SIDECAR_NAME)
        .map_err(|error| error.to_string())
}

async fn extractor_version(app: &AppHandle) -> Result<String, String> {
    let output = extractor_command(app)?
        .arg("-v")
        .output()
        .await
        .map_err(|error| error.to_string())?;

    let mut text = String::from_utf8_lossy(&output.stdout).into_owned();
    text.push_str(&String::from_utf8_lossy(&output.stderr));

    if !output.status.success() {
        return Err("sacd_extract version check failed.".to_string());
    }

    parse_extractor_version(&text)
        .ok_or_else(|| "sacd_extract returned an invalid version.".to_string())
}

#[tauri::command]
async fn select_iso(app: AppHandle) -> Option<String> {
    let preferences = read_preferences(&app);
    let mut dialog = app
        .dialog()
        .file()
        .set_title("Select SACD ISO")
        .add_filter("SACD ISO images", &["iso"])
        .add_filter("All files", &["*"]);

    if let Some(directory) = preferences.last_iso_directory.filter(|path| path.is_dir()) {
        dialog = dialog.set_directory(directory);
    }

    let selected = dialog.blocking_pick_file()?.into_path().ok()?;

    save_preferences(
        &app,
        &Preferences {
            last_iso_directory: selected.parent().map(Path::to_path_buf),
        },
    );

    Some(selected.to_string_lossy().into_owned())
}

#[tauri::command]
async fn select_output(app: AppHandle) -> Option<String> {
    app.dialog()
        .file()
        .set_title("Select output directory")
        .blocking_pick_folder()
        .and_then(|path| path.into_path().ok())
        .map(|path| path.to_string_lossy().into_owned())
}

#[tauri::command]
async fn check_engine(app: AppHandle) -> StatusPayload {
    match extractor_version(&app).await {
        Ok(version) => StatusPayload {
            state: "engine-ready".to_string(),
            message: None,
            version: Some(version),
            exit_code: None,
            signal: None,
        },
        Err(message) => StatusPayload {
            state: "engine-error".to_string(),
            message: Some(message),
            version: None,
            exit_code: None,
            signal: None,
        },
    }
}

fn confirm_overwrite(app: &AppHandle, output: &Path) -> Result<bool, String> {
    let existing = collect_audio_files(output);

    if existing.is_empty() {
        return Ok(true);
    }

    let confirmed = app
        .dialog()
        .message(format!(
            "{} DSF/DFF files already exist in the destination.\n\n\
             Remove the existing audio files and create new ones?\n\n{}",
            existing.len(),
            output.display()
        ))
        .title("Audio files already exist")
        .kind(MessageDialogKind::Warning)
        .buttons(MessageDialogButtons::OkCancelCustom(
            "Overwrite".to_string(),
            "Cancel".to_string(),
        ))
        .blocking_show();

    if !confirmed {
        return Ok(false);
    }

    for path in existing {
        fs::remove_file(&path)
            .map_err(|error| format!("Unable to remove {}: {error}", path.display()))?;
    }

    Ok(true)
}

fn start_track_monitor(app: AppHandle, output: PathBuf, process_id: u64) {
    std::thread::spawn(move || {
        let mut seen = HashSet::new();
        let mut track_number = 0usize;

        loop {
            for path in collect_audio_files(&output) {
                if !seen.insert(path.clone()) {
                    continue;
                }

                track_number += 1;
                let payload = TrackPayload {
                    number: track_number,
                    filename: path
                        .file_name()
                        .map(|name| name.to_string_lossy().into_owned())
                        .unwrap_or_default(),
                    path: path.to_string_lossy().into_owned(),
                };
                let _ = app.emit("sacd-track", payload);
            }

            let active = app
                .state::<ApplicationState>()
                .inner
                .lock()
                .map(|state| {
                    state
                        .running
                        .as_ref()
                        .is_some_and(|running| running.id == process_id)
                })
                .unwrap_or(false);

            if !active {
                break;
            }

            std::thread::sleep(Duration::from_millis(500));
        }
    });
}

#[tauri::command]
async fn start_extraction(app: AppHandle, options: ExtractionOptions) -> CommandResponse {
    if app
        .state::<ApplicationState>()
        .inner
        .lock()
        .map(|state| state.running.is_some())
        .unwrap_or(true)
    {
        return CommandResponse::error("An extraction is already running.");
    }

    if let Err(error) = validate_extraction_paths(&options) {
        return CommandResponse::error(error);
    }

    match confirm_overwrite(&app, Path::new(&options.output)) {
        Ok(true) => {}
        Ok(false) => return CommandResponse::cancelled(),
        Err(error) => return CommandResponse::error(error),
    }

    if let Err(error) = extractor_version(&app).await {
        return CommandResponse::error(error);
    }

    let arguments = build_extractor_arguments(&options);
    let command = match extractor_command(&app) {
        Ok(command) => command.args(&arguments),
        Err(error) => return CommandResponse::error(error),
    };
    let (mut receiver, child) = match command.spawn() {
        Ok(process) => process,
        Err(error) => return CommandResponse::error(error.to_string()),
    };

    let process_id = {
        let application_state = app.state::<ApplicationState>();
        let Ok(mut state) = application_state.inner.lock() else {
            let _ = child.kill();
            return CommandResponse::error("Application process state is unavailable.");
        };

        if state.running.is_some() {
            let _ = child.kill();
            return CommandResponse::error("An extraction is already running.");
        }

        state.next_id += 1;
        let process_id = state.next_id;
        state.running = Some(RunningProcess {
            id: process_id,
            child,
        });
        process_id
    };

    let _ = app.emit(
        "sacd-status",
        StatusPayload {
            state: "running".to_string(),
            message: Some("Extraction started".to_string()),
            version: None,
            exit_code: None,
            signal: None,
        },
    );

    let displayed = std::iter::once(SIDECAR_NAME.to_string())
        .chain(arguments.iter().map(|argument| {
            if argument.contains(' ') {
                format!("\"{argument}\"")
            } else {
                argument.clone()
            }
        }))
        .collect::<Vec<_>>()
        .join(" ");
    let _ = app.emit("sacd-output", format!("Starting:\n{displayed}\n\n"));

    start_track_monitor(app.clone(), PathBuf::from(&options.output), process_id);

    tauri::async_runtime::spawn(async move {
        while let Some(event) = receiver.recv().await {
            match event {
                CommandEvent::Stdout(bytes) | CommandEvent::Stderr(bytes) => {
                    let text = String::from_utf8_lossy(&bytes);
                    let _ = app.emit("sacd-output", format!("{text}\n"));
                }
                CommandEvent::Error(message) => {
                    let _ = app.emit("sacd-output", format!("\nError: {message}\n"));
                }
                CommandEvent::Terminated(payload) => {
                    let cancelled = app
                        .state::<ApplicationState>()
                        .inner
                        .lock()
                        .map(|mut state| {
                            if state
                                .running
                                .as_ref()
                                .is_some_and(|running| running.id == process_id)
                            {
                                state.running = None;
                            }

                            if state.cancelled_id == Some(process_id) {
                                state.cancelled_id = None;
                                true
                            } else {
                                false
                            }
                        })
                        .unwrap_or(false);

                    let mut status = classify_result(payload.code, cancelled);
                    status.signal = payload.signal;
                    let _ = app.emit("sacd-status", status);
                    let _ = app.emit(
                        "sacd-output",
                        format!(
                            "\nProcess finished with exit code {}.\n",
                            payload
                                .code
                                .map(|code| code.to_string())
                                .unwrap_or_else(|| "unknown".to_string())
                        ),
                    );
                    break;
                }
                _ => {}
            }
        }
    });

    CommandResponse::success(arguments)
}

#[tauri::command]
fn stop_extraction(state: State<'_, ApplicationState>) -> CommandResponse {
    let running = {
        let Ok(mut state) = state.inner.lock() else {
            return CommandResponse::error("Application process state is unavailable.");
        };

        let Some(running) = state.running.take() else {
            return CommandResponse::error("No extraction is running.");
        };

        state.cancelled_id = Some(running.id);
        running
    };

    match running.child.kill() {
        Ok(()) => CommandResponse::stopped(),
        Err(error) => CommandResponse::error(error.to_string()),
    }
}

fn terminate_running_process(app: &AppHandle) {
    let running = app
        .state::<ApplicationState>()
        .inner
        .lock()
        .ok()
        .and_then(|mut state| state.running.take());

    if let Some(running) = running {
        let _ = running.child.kill();
    }
}

pub fn run() {
    let application = tauri::Builder::default()
        .plugin(tauri_plugin_dialog::init())
        .plugin(tauri_plugin_shell::init())
        .manage(ApplicationState::default())
        .invoke_handler(tauri::generate_handler![
            select_iso,
            select_output,
            check_engine,
            start_extraction,
            stop_extraction,
        ])
        .build(tauri::generate_context!())
        .expect("failed to build sacd-extract-gui");

    application.run(|app, event| {
        if matches!(event, RunEvent::Exit | RunEvent::ExitRequested { .. }) {
            terminate_running_process(app);
        }
    });
}

#[cfg(test)]
mod tests {
    use super::*;

    fn options() -> ExtractionOptions {
        ExtractionOptions {
            source: "/music/Album.iso".to_string(),
            output: "/music/output".to_string(),
            format: "dsf".to_string(),
            area: "both".to_string(),
            decode_dst: true,
            export_cuesheet: true,
        }
    }

    #[test]
    fn builds_current_long_form_extractor_arguments() {
        assert_eq!(
            build_extractor_arguments(&options()),
            vec![
                "--input",
                "/music/Album.iso",
                "--dsf",
                "--track-output-dir",
                "/music/output",
                "--stereo",
                "--multichannel",
                "--convert-dst",
                "--cue",
            ]
        );
    }

    #[test]
    fn builds_multichannel_dsdiff_arguments() {
        let mut options = options();
        options.format = "dff".to_string();
        options.area = "multichannel".to_string();
        options.decode_dst = false;
        options.export_cuesheet = false;

        assert_eq!(
            build_extractor_arguments(&options),
            vec![
                "--input",
                "/music/Album.iso",
                "--dsdiff",
                "--track-output-dir",
                "/music/output",
                "--multichannel",
            ]
        );
    }

    #[test]
    fn parses_the_extractor_version() {
        assert_eq!(
            parse_extractor_version("sacd_extract client 0.5.0\n"),
            Some("0.5.0".to_string())
        );
        assert_eq!(parse_extractor_version("invalid"), None);
    }

    #[test]
    fn maps_the_extractor_exit_contract() {
        assert_eq!(classify_result(Some(0), false).state, "completed");
        assert_eq!(classify_result(Some(1), false).state, "partial");
        assert_eq!(classify_result(Some(2), false).state, "error");
        assert_eq!(classify_result(Some(130), false).state, "cancelled");
        assert_eq!(classify_result(None, true).state, "cancelled");
    }

    #[test]
    fn recognizes_output_audio_extensions_case_insensitively() {
        assert!(is_audio_file(Path::new("Track.dsf")));
        assert!(is_audio_file(Path::new("Track.DFF")));
        assert!(!is_audio_file(Path::new("Album.iso")));
    }
}
