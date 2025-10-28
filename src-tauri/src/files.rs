use std::{
    env, fs, io::Write,
    path::PathBuf, process::Command
};
use serde::{Deserialize, Serialize};
#[cfg(target_os = "windows")]
use uuid::Uuid;

#[derive(Debug, Deserialize, Serialize, PartialEq, Default)]
pub(crate) struct SoundboardSFX {
    pub(crate) name: String,
    pub(crate) icon: String,
    pub(crate) color: [u8; 3],
    pub(crate) sound: String,
    pub(crate) lowlatency: bool
}

#[derive(Debug, Deserialize, Serialize, PartialEq, Clone, Default)]
pub(crate) struct Channel {
    pub(crate) name: String,
    pub(crate) icon: String,
    pub(crate) color: [u8; 3],
    pub(crate) device: String,
    pub(crate) deviceorapp: bool,
    pub(crate) lowlatency: bool,
    pub(crate) volume: f32
}

#[derive(Deserialize, Serialize, PartialEq, Clone, Default)]
pub(crate) struct Settings {
    pub(crate) output: String,
    pub(crate) scale: f32,
    pub(crate) light: bool
}

#[derive(Deserialize, Serialize, Default)]
pub(crate) struct File {
    pub(crate) soundboard: Vec<SoundboardSFX>,
    pub(crate) channels: Vec<Channel>,
    pub(crate) settings: Settings,
}

#[cfg(target_os = "windows")]
const EMBEDDED_BIN: &[u8] = include_bytes!("../updater/target/release/updater.exe");

#[cfg(target_os = "linux")]
const EMBEDDED_BIN: &[u8] = include_bytes!("../updater/target/release/updater");

fn app_base() -> PathBuf {
    #[cfg(target_os = "windows")]
    let base: String = env::var("APPDATA").unwrap_or("Error occured when getting App Base".to_string());
    #[cfg(target_os = "linux")]
    let base: String = env::var("XDG_DATA_HOME")
        .unwrap_or_else(|_| format!("{}/.local/share", env::var("HOME").unwrap_or_else(|_| "/tmp".into())));
    
    PathBuf::from(base).join("Vice")
}

fn settings_json() -> PathBuf {
    app_base().join("settings.json")
}

pub(crate) fn create_files() {
    let base: PathBuf = app_base();

    if !base.exists() {
        if let Err(e) = fs::create_dir_all(base) {
            println!("Failed to create app base: {e}");
        }
    }

    let file_path: PathBuf = settings_json();

    if !file_path.exists() {
        let mut default_file: File = File::default();
        default_file.settings.scale = 1.0;

        match serde_json::to_string_pretty(&default_file) {
            Ok(json) => {
                if let Err(e) = fs::write(&file_path, json) {
                    eprintln!("Failed to create settings: {e}");
                }
            }
            Err(e) => {
                eprintln!("Failed to serialize default settings: {e}");
            }
        }
    }
}

pub(crate) fn get_file() -> File {
    let path: PathBuf = settings_json();

    if path.exists() {
        let data = match fs::read_to_string(path) {
            Ok(d) => d,
            Err(e) => {
                eprintln!("Failed to read settings file: {}", e);
                return File::default();
            }
        };

        if data.is_empty() {
            return File::default();
        }

        match serde_json::from_str::<File>(&data) {
            Ok(file) => return file,
            Err(e) => {
                eprintln!("Failed to parse settings file: {}", e);
                return File::default();
            }
        }
    }

    File::default()
}

pub(crate) fn save_file(file: &File) -> Result<(), String> {
    let path: PathBuf = settings_json();

    let data: String = match serde_json::to_string_pretty(file) {
        Ok(d) => d,
        Err(e) => {
            eprintln!("Failed to serialize settings: {}", e);
            return Err("Serilization".to_string());
        }
    };

    match fs::write(&path, data) {
        Ok(_) => Ok(()),
        Err(e) => {
            eprintln!("Failed to write settings file: {}", e);
            return Err("Saving".to_string());
        }
    }
}

pub(crate) fn get_channels() -> Vec<Channel> {
    get_file().channels
}

pub(crate) fn save_channels(channels: Vec<Channel>) -> Result<(), String> {
    let mut file: File = get_file();
    file.channels = channels;
    return save_file(&file);
}

pub(crate) fn get_soundboard() -> Vec<SoundboardSFX> {
    get_file().soundboard
}

pub(crate) fn save_soundboard(soundboard_sfxs: Vec<SoundboardSFX>) -> Result<(), String> {
    let mut file: File = get_file();
    file.soundboard = soundboard_sfxs;
    return save_file(&file);
}

pub(crate) fn get_settings() -> Settings {
    get_file().settings
}

pub(crate) fn save_settings(settings: Settings) -> Result<(), String> {
    let mut file: File = get_file();
    file.settings = settings;
    return save_file(&file);
}

pub(crate) fn extract_updater(arg: &str, path: PathBuf, debug: &str) -> Result<String, String> {
    let mut temp_path = env::temp_dir();

    #[cfg(target_os = "windows")]
    let filename = "Vice-Uninstaller-".to_string()+&Uuid::new_v4().to_string()+".exe";
    #[cfg(any(target_os = "linux"))]
    let filename = "Vice-Uninstaller-".to_string()+&Uuid::new_v4().to_string();

    temp_path.push(&filename);

    let mut file = fs::File::create(&temp_path).unwrap();
    let _ = file.write_all(EMBEDDED_BIN);

    #[cfg(unix)]
    {
        let perms = fs::metadata(&temp_path)
            .map_err(|e| e.to_string())?
            .permissions();

        fs::set_permissions(&temp_path, perms).map_err(|e| e.to_string())?;
    }

    let _ = Command::new("cmd")
        .args(["/C", "start", "", &temp_path.to_string_lossy().to_string(), arg, &path.to_string_lossy().to_string(), debug])
        .spawn();

    Ok(filename)
}