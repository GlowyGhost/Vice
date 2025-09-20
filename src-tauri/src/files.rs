use std::{
    env,
    path::PathBuf,
    fs
};
use serde::{Deserialize, Serialize};


#[derive(Debug, Deserialize, Serialize, PartialEq)]
pub(crate) struct SoundboardSFX {
    pub(crate) name: String,
    pub(crate) icon: String,
    pub(crate) color: [u8; 3],
    pub(crate) sound: String
}

#[derive(Debug, Deserialize, Serialize, PartialEq)]
pub(crate) struct Channel {
    pub(crate) name: String,
    pub(crate) icon: String,
    pub(crate) color: [u8; 3],
    pub(crate) device: String
}

#[derive(Deserialize, Serialize)]
pub(crate) struct Settings {
    pub(crate) soundboard: Vec<SoundboardSFX>,
    pub(crate) channels: Vec<Channel>
}

fn app_base() -> PathBuf {
    #[cfg(target_os = "windows")]
    let base = env::var("APPDATA");
    #[cfg(target_os = "linux")]
    let base = env::var("XDG_DATA_HOME");
    
    PathBuf::from(
        base.unwrap_or("Error occured when getting App Base".to_string())
    ).join("Vice")
}

fn settings_json() -> PathBuf {
    app_base().join("settings.json")
}

pub(crate) fn create_files() {
    let base = app_base();

    if !base.exists() {
        if let Err(e) = fs::create_dir_all(base) {
            println!("Failed to create app base: {e}");
        }
    }

    let settings_path = settings_json();

    if !settings_path.exists() {
        let default_settings = r#"{
    "soundboard": [],
    "channels": []
}"#;

        if let Err(e) = fs::write(&settings_path, default_settings) {
            println!("Failed to create settings: {e}");
        }
    }
}

pub(crate) fn get_settings() -> Result<Option<Settings>, String> {
    let path = settings_json();

    if path.exists() {
        let data = fs::read_to_string(path)
            .map_err(|e| format!("Failed to read settings file: {}", e))?;

        if data.is_empty() {
            return Ok(None)
        }

        let settings: Settings = serde_json::from_str(&data)
            .map_err(|e| format!("Failed to parse settings file: {}", e))?;
        return Ok(Some(settings));
    }

    Ok(None)
}

pub(crate) fn save_settings(settings: &Settings) -> Result<(), String> {
    let path = settings_json();

    let json_data = serde_json::to_string_pretty(settings)
        .map_err(|e| format!("Failed to serialize settings: {}", e))?;

    fs::write(&path, json_data)
        .map_err(|e| format!("Failed to write settings file: {}", e))?;

    Ok(())
}

pub(crate) fn get_channels() -> Option<Vec<Channel>> {
    match get_settings().unwrap() {
        None => {return None},
        Some(data) => {return Some(data.channels)} 
    }
}

pub(crate) fn save_channels(channels: Vec<Channel>) -> Result<(), String> {
    match get_settings().unwrap() {
        None => {return Ok(())},
        Some(mut data) => {
            data.channels = channels;
            return save_settings(&data);
        } 
    }
}

pub(crate) fn get_soundboard() -> Option<Vec<SoundboardSFX>> {
    match get_settings().unwrap() {
        None => {return None},
        Some(data) => {return Some(data.soundboard)} 
    }
}

pub(crate) fn save_soundboard(soundboard_sfxs: Vec<SoundboardSFX>) -> Result<(), String> {
    match get_settings().unwrap() {
        None => {return Ok(())},
        Some(mut data) => {
            data.soundboard = soundboard_sfxs;
            return save_settings(&data);
        } 
    }
}