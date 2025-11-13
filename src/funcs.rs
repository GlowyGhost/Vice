use std::net::TcpStream;
use std::time::Duration;
use rfd::{MessageDialog, MessageDialogResult};
use serde::Deserialize;

use crate::files::{self, Channel, Settings, SoundboardSFX};
use crate::audio::{self};
use crate::performance;

pub(crate) fn get_soundboard() -> Vec<SoundboardSFX> {
    files::get_soundboard()
}

pub(crate) fn get_channels() -> Vec<Channel> {
    files::get_channels()
}

pub(crate) fn new_channel(color: [u8; 3], icon: String, name: String, deviceapps: String, device: bool, low: bool) -> Result<(), String> {
    let channel: Channel = Channel{name, icon, color, device: deviceapps, deviceorapp: device, lowlatency: low, volume: 1.0};
    let mut channels: Vec<Channel> = files::get_channels();

    channels.push(channel);
    return files::save_channels(channels).map(|_| audio::restart());
}

pub(crate) fn new_sound(color: [u8; 3], icon: String, name: String, sound: String, low: bool) -> Result<(), String> {
    let sfx: SoundboardSFX = SoundboardSFX{name, icon, color, sound, lowlatency: low};
    let mut sfxs: Vec<SoundboardSFX> = files::get_soundboard();

    sfxs.push(sfx);
    return files::save_soundboard(sfxs);
}

pub(crate) fn edit_channel(color: [u8; 3], icon: String, name: String, deviceapps: String, device: bool, oldname: String, low: bool) -> Result<(), String> {
    let mut channels: Vec<Channel> = files::get_channels();

    if let Some(pos) = channels.iter().position(|c: &Channel| c.name == oldname) {
        channels[pos] = Channel{name, icon, color, device: deviceapps, deviceorapp: device, lowlatency: low, volume: 1.0};
    } else {
        eprintln!("Channel '{}' not found", oldname);
        return Err(format!("Channel '{}' not found", oldname));
    }
    
    return files::save_channels(channels).map(|_| audio::restart());
}

pub(crate) fn edit_soundboard(color: [u8; 3], icon: String, name: String, sound: String, oldname: String, low: bool) -> Result<(), String> {
    let mut sfxs: Vec<SoundboardSFX> = files::get_soundboard();

    if let Some(pos) = sfxs.iter().position(|c: &SoundboardSFX| c.name == oldname) {
        sfxs[pos] = SoundboardSFX{name, icon, color, sound, lowlatency: low};
    } else {
        eprintln!("Soundeffect '{}' not found", oldname);
        return Err(format!("Soundeffect '{}' not found", oldname));
    }
    
    return files::save_soundboard(sfxs);
}

pub(crate) fn delete_channel(name: String) -> Result<(), String> {
    let mut channels: Vec<Channel> = files::get_channels();

    if let Some(pos) = channels.iter().position(|c| c.name == name) {
        channels.remove(pos);
    } else {
        return Err(format!("Channel '{}' not found", name));
    }
    
    return files::save_channels(channels).map(|_| audio::restart());
}

pub(crate) fn delete_sound(name: String) -> Result<(), String> {
    let mut sfxs: Vec<SoundboardSFX> = files::get_soundboard();

    if let Some(pos) = sfxs.iter().position(|c: &SoundboardSFX| c.name == name) {
        sfxs.remove(pos);
    } else {
        return Err(format!("Soundeffect '{}' not found", name));
    }
    
    return files::save_soundboard(sfxs);
}

pub(crate) fn pick_menu_sound() -> Option<String> {
    #[cfg(target_os = "windows")]
    let formats: [&str; 6] = ["wav", "mp3", "wma", "aac", "m4a", "flac"];
    #[cfg(target_os = "linux")]
    let formats: [&str; 11] = ["wav", "mp3", "wma", "aac", "m4a", "flac", "ogg", "opus", "alac", "aiff", "aif"];

    if let Some(path) = rfd::FileDialog::new()
        .add_filter("Sound Files", &formats)
        .pick_file()
    {
        Some(path.to_string_lossy().to_string())
    } else {
        None
    }
}

pub(crate) fn get_devices() -> Vec<String> {
    audio::inputs()
}

pub(crate) fn get_apps() -> Vec<String> {
    audio::apps()
}

pub(crate) fn save_settings(output: String, scale: f32, light: bool, monitor: bool, peaks: bool) -> Result<(), String> {
    let mut settings: Settings = files::get_settings();
    settings.output = output;
    settings.scale = scale;
    settings.light = light;
    settings.monitor = monitor;
    settings.peaks = peaks;

    files::save_settings(settings).map(|_| {audio::restart(); performance::change_bool(monitor)})
}

pub(crate) fn get_performance() -> String {
    serde_json::to_string(&performance::get_data()).unwrap_or_else(|_| "{}".to_string())
}

pub(crate) fn clear_performance() {
    performance::clear_data();
}

pub(crate) fn get_settings() -> Settings {
    files::get_settings()
}

pub(crate) fn set_volume(name: String, volume: f32) {
    let mut channels: Vec<Channel> = files::get_channels();

    if let Some(pos) = channels.iter().position(|c: &Channel| c.name == name) {
        let mut channel: Channel = channels[pos].clone();
        channel.volume = volume;
        channels[pos] = channel.clone();

        audio::set_volume(channel.name, volume);
    } else {
        eprintln!("Channel '{}' not found", name);
        return;
    }

    files::save_channels(channels).unwrap_or_else(|e| eprintln!("Error saving channels: {}", e));
}

pub(crate) fn get_outputs() -> Vec<String> {
    audio::outputs()
}

pub(crate) fn play_sound(sound: String, low: bool) {
    audio::play_sfx(&sound, low);
}

pub(crate) fn get_volume(name: String, get: bool, device: bool) -> String {
    audio::get_volume_parsed(name, get, device)
}

pub(crate) fn uninstall() -> Result<String, String> {
    let res: MessageDialogResult = MessageDialog::new()
        .set_title("Uninstall")
        .set_description("Are you sure you want to uninstall Luauncher?")
        .set_buttons(rfd::MessageButtons::YesNo)
        .show();

    if res == MessageDialogResult::Yes {
        return files::extract_updater("uninstall", std::env::current_exe().unwrap(), "false");
    }
    Ok("Undid".to_string())
}

pub(crate) fn update() -> Result<String, String> {
    #[derive(Deserialize)]
    struct Release {
        name: String
    }

    let connected = TcpStream::connect_timeout(
        &("1.1.1.1:80".parse().unwrap()),
        Duration::from_secs(2)
    ).is_ok();

    if connected == false {
        MessageDialog::new()
            .set_title("No Internet Connection")
            .set_description("It appears there is no internet connection.")
            .set_buttons(rfd::MessageButtons::Ok)
            .show();

        return Ok("No Internet".to_string());
    }

    let url = "https://api.github.com/repos/GlowyGhost/Vice/releases/latest";

    let client = reqwest::blocking::Client::new();
    let mut res = client
        .get(url)
        .header("User-Agent", "Vice-app")
        .send()
        .map_err(|e| e.to_string())?
        .json::<Release>()
        .map_err(|e| e.to_string())?;

    res.name.remove(0);

    if res.name == env!("CARGO_PKG_VERSION") {
        MessageDialog::new()
            .set_title("No Update")
            .set_description("It appears there is no update available.")
            .set_buttons(rfd::MessageButtons::Ok)
            .show();

        return Ok("No Update".to_string());
    }

    let res_msg = MessageDialog::new()
        .set_title("Update")
        .set_description(format!("Are you sure you want to update Vice from {} to {}?", env!("CARGO_PKG_VERSION"), res.name))
        .set_buttons(rfd::MessageButtons::YesNo)
        .show();

    if res_msg == MessageDialogResult::Yes {
        #[cfg(debug_assertions)]
        let debug = "true";

        #[cfg(not(debug_assertions))]
        let debug = "false";

        return files::extract_updater("update", std::env::current_exe().unwrap(), debug);
    }
    Ok("Undid".to_string())
}