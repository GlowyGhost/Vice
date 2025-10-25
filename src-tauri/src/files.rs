use std::{
    env, fs, path::PathBuf
};
use serde::{Deserialize, Serialize};

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

#[derive(Deserialize, Serialize, Default)]
pub(crate) struct Settings {
    pub(crate) soundboard: Vec<SoundboardSFX>,
    pub(crate) channels: Vec<Channel>,
    pub(crate) output: String,
}

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

    let settings_path: PathBuf = settings_json();

    if !settings_path.exists() {
        let default_settings: Settings = Settings::default();

        match serde_json::to_string_pretty(&default_settings) {
            Ok(json) => {
                if let Err(e) = fs::write(&settings_path, json) {
                    eprintln!("Failed to create settings: {e}");
                }
            }
            Err(e) => {
                eprintln!("Failed to serialize default settings: {e}");
            }
        }
    }
}

pub(crate) fn get_settings() -> Settings {
    let path: PathBuf = settings_json();

    if path.exists() {
        let data = match fs::read_to_string(path) {
            Ok(d) => d,
            Err(e) => {
                eprintln!("Failed to read settings file: {}", e);
                return Settings::default();
            }
        };

        if data.is_empty() {
            return Settings::default();
        }

        match serde_json::from_str::<Settings>(&data) {
            Ok(settings) => return settings,
            Err(e) => {
                eprintln!("Failed to parse settings file: {}", e);
                return Settings::default();
            }
        }
    }

    Settings::default()
}

pub(crate) fn save_settings(settings: &Settings) -> Result<(), String> {
    let path: PathBuf = settings_json();

    let data: String = match serde_json::to_string_pretty(settings) {
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
    get_settings().channels
}

pub(crate) fn save_channels(channels: Vec<Channel>) -> Result<(), String> {
    let mut settings: Settings = get_settings();
    settings.channels = channels;
    return save_settings(&settings);
}

pub(crate) fn get_soundboard() -> Vec<SoundboardSFX> {
    get_settings().soundboard
}

pub(crate) fn get_output() -> String {
    get_settings().output
}

pub(crate) fn save_soundboard(soundboard_sfxs: Vec<SoundboardSFX>) -> Result<(), String> {
    let mut settings: Settings = get_settings();
    settings.soundboard = soundboard_sfxs;
    return save_settings(&settings);
}