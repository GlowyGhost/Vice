use std::{
    env, fs, io::Write,
    path::PathBuf, process::Command
};
use serde::{Deserialize, Serialize};
use serde_json::Value;
use uuid::Uuid;

#[derive(Debug, Deserialize, Serialize, PartialEq, Default)]
pub(crate) struct SoundboardSFX {
    pub(crate) name: String,
    pub(crate) icon: String,
    pub(crate) color: [u8; 3],
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

#[derive(Deserialize, Serialize, PartialEq, Clone)]
pub(crate) struct Settings {
    pub(crate) output: String,
    pub(crate) scale: f32,
    pub(crate) light: bool,
    pub(crate) monitor: bool,
    pub(crate) peaks: bool
}

#[derive(Deserialize, Serialize)]
pub(crate) struct File {
    pub(crate) soundboard: Vec<SoundboardSFX>,
    pub(crate) channels: Vec<Channel>,
    pub(crate) settings: Settings,
}

impl Default for File {
    fn default() -> Self {
        File { soundboard: vec![], channels: vec![], settings: Settings::default() }
    }
}

impl Default for Settings {
    fn default() -> Self {
        Settings { output: "".to_string(), scale: 1.0, light: false, monitor: true, peaks: true }
    }
}

const EMBEDDED_BIN: &[u8] = include_bytes!("../updater/target/release/updater.exe");

pub(crate) fn app_base() -> PathBuf {
    let base = env::var("APPDATA");

    match base {
        Ok(s) => return PathBuf::from(s).join("Vice"),
        Err(e) => {
            eprintln!("Error occured when getting App Base: {:#?}", e);
            return env::temp_dir();
        }
    }
}

pub(crate) fn sfx_base() -> PathBuf {
    let base = env::var("APPDATA");

    match base {
        Ok(s) => return PathBuf::from(s).join("Vice").join("SFXs"),
        Err(e) => {
            eprintln!("Error occured when getting SFXs Base: {:#?}", e);
            return env::temp_dir().join("SFXs");
        }
    }
}

fn settings_json() -> PathBuf {
    app_base().join("settings.json")
}

pub(crate) fn create_files() {
    let base: PathBuf = app_base();

    if !base.exists() {
        if let Err(e) = fs::create_dir_all(base) {
            eprintln!("Failed to create app base: {}", e);
        }
    }

    let file_path: PathBuf = settings_json();

    if !file_path.exists() {
        let mut default_file: File = File::default();
        default_file.settings.scale = 1.0;

        match serde_json::to_string_pretty(&default_file) {
            Ok(json) => {
                if let Err(e) = fs::write(&file_path, json) {
                    eprintln!("Failed to create settings: {}", e);
                }
            }
            Err(e) => {
                eprintln!("Failed to serialize default settings: {}", e);
            }
        }
    }

    let sfxs_path: PathBuf = sfx_base();

    if !sfxs_path.exists() {
        if let Err(e) = fs::create_dir_all(sfxs_path) {
            eprintln!("Failed to create soundeffect directory: {}", e);
        }
    }
}

pub(crate) fn fix_settings(broken: Value) -> Settings {
    let mut settings: Settings = Settings::default();

    if let Some(output) = broken.get("output").and_then(|v| v.as_str()) {
        settings.output = output.to_string();
    }

    if let Some(scale) = broken.get("scale").and_then(|v| v.as_f64()) {
        if scale.is_finite() && scale >= 0.1 && scale <= 2.0 {
            settings.scale = scale as f32;
        } else {
            settings.scale = 1.0;
        }
    }

    if let Some(light) = broken.get("light").and_then(|v| v.as_bool()) {
        settings.light = light;
    }

    if let Some(monitor) = broken.get("monitor").and_then(|v| v.as_bool()) {
        settings.monitor = monitor;
    }

    if let Some(peaks) = broken.get("peaks").and_then(|v| v.as_bool()) {
        settings.peaks = peaks;
    }

    settings
}

pub(crate) fn fix_soundeffect(broken: Value) -> SoundboardSFX {
    let mut sfx: SoundboardSFX = SoundboardSFX::default();

    if let Some(name) = broken.get("name").and_then(|v| v.as_str()) {
        sfx.name = name.to_string();
    }

    if let Some(icon) = broken.get("icon").and_then(|v| v.as_str()) {
        sfx.icon = icon.to_string();
    }

    if let Some(broken_color) = broken.get("color").and_then(|v| v.as_array()) {
        let mut color: Vec<u8> = vec![];
        for v in broken_color {
            let int: i64 = v.as_i64().map_or(-1, |i: i64| i);
            if int > 255 {
                color.push(255 as u8);
            } else if int < 0 {
                color.push(0 as u8);
            } else {
                color.push(int as u8);
            }
        }

        if color.len() > 3 {
            color.truncate(3);
        } else if color.len() < 3 {
            color.resize(3, 0); 
        }

        let mut color_array: [u8; 3] = [0, 0, 0];
        for (i, &val) in color.iter().enumerate() {
            color_array[i] = val;
        }

        sfx.color = color_array;
    }

    if let Some(lowlatency) = broken.get("lowlatency").and_then(|v| v.as_bool()) {
        sfx.lowlatency = lowlatency;
    }

    sfx
}

pub(crate) fn fix_channel(broken: Value) -> Channel {
    let mut channel: Channel = Channel::default();

    if let Some(name) = broken.get("name").and_then(|v| v.as_str()) {
        channel.name = name.to_string();
    }

    if let Some(icon) = broken.get("icon").and_then(|v| v.as_str()) {
        channel.icon = icon.to_string();
    }

    if let Some(broken_color) = broken.get("color").and_then(|v| v.as_array()) {
        let mut color: Vec<u8> = vec![];
        for v in broken_color {
            let int: i64 = v.as_i64().map_or(-1, |i: i64| i);
            if int > 255 {
                color.push(255 as u8);
            } else if int < 0 {
                color.push(0 as u8);
            } else {
                color.push(int as u8);
            }
        }

        if color.len() > 3 {
            color.truncate(3);
        } else if color.len() < 3 {
            color.resize(3, 0); 
        }

        let mut color_array: [u8; 3] = [0, 0, 0];
        for (i, &val) in color.iter().enumerate() {
            color_array[i] = val;
        }

        channel.color = color_array;
    }

    if let Some(device) = broken.get("device").and_then(|v| v.as_str()) {
        channel.device = device.to_string();
    }

    if let Some(deviceorapp) = broken.get("deviceorapp").and_then(|v| v.as_bool()) {
        channel.deviceorapp = deviceorapp;
    }

    if let Some(lowlatency) = broken.get("lowlatency").and_then(|v| v.as_bool()) {
        channel.lowlatency = lowlatency;
    }

    if let Some(volume) = broken.get("volume").and_then(|v| v.as_f64()) {
        channel.volume = volume as f32;
    }

    channel
}

pub(crate) fn fix_file(broken: Value) -> File {
    let mut file: File = File::default();

    if let Some(settings_val) = broken.get("settings") {
        file.settings = fix_settings(settings_val.clone());
    }

    if let Some(sfxs) = broken.get("soundboard").and_then(|v| v.as_array()) {
        let mut soundeffects: Vec<SoundboardSFX> = vec![];
        for sfx in sfxs.iter() {
            soundeffects.push(fix_soundeffect(sfx.clone()));
        }
        file.soundboard = soundeffects;
    }

    if let Some(channels) = broken.get("channels").and_then(|v| v.as_array()) {
        let mut cs: Vec<Channel> = vec![];
        for channel in channels.iter() {
            cs.push(fix_channel(channel.clone()));
        }
        file.channels = cs;
    }

    file
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
                println!("Attempting to fix...");
                match serde_json::from_str::<Value>(&data) {
                    Ok(value) => {
                        let fixed: File = fix_file(value);
                        println!("Successfully fixed!");
                        let _ = save_file(&fixed);
                        return fixed;
                    }
                    Err(e) => {
                        let fixed: File = File::default();
                        println!("Failed to fix the settings file: {}", e);
                        let _ = save_file(&fixed);
                        return fixed;
                    }
                }
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

    let filename = "Vice-Uninstaller-".to_string()+&Uuid::new_v4().to_string()+".exe";

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