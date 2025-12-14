use std::path::Path;
use std::{net::TcpStream, time::Duration, fs};
use rfd::{MessageDialog, MessageDialogResult};
use serde::Deserialize;

use crate::files::{self, Channel, Settings, SoundboardSFX};
use crate::audio::{self};
use crate::performance;

static SFX_EXTENTIONS: [&str; 6] = ["wav", "mp3", "wma", "aac", "m4a", "flac"];

pub(crate) fn new_channel(color: [u8; 3], icon: String, name: String, deviceapps: String, device: bool, low: bool) -> Result<(), String> {
    let channel: Channel = Channel{name, icon, color, device: deviceapps, deviceorapp: device, lowlatency: low, volume: 1.0};
    let mut channels: Vec<Channel> = files::get_channels();

    channels.push(channel);
    return files::save_channels(channels).map(|_| audio::restart());
}

pub(crate) fn new_sound(color: [u8; 3], icon: String, name: String, sound: String, low: bool) -> Result<(), String> {
    let mut sfxs: Vec<SoundboardSFX> = files::get_soundboard();
    
    let ext = Path::new(&sound)
        .extension();

    match ext {
        None => {
            eprintln!("Failed to get extnetion for soundeffect \"{}\"", name);
            return Err(format!("Failed to get extnetion for soundeffect \"{}\"", name));
        }
        Some(o) => {
            if let Err(e) = fs::copy(&sound, files::sfx_base().join(format!("{}.{}", name, o.to_str().unwrap_or("wav")).to_string())) {
                eprintln!("Failed to copy soundeffect for soundeffect \"{}\": {:#?}", name, e);
                return Err(format!("Failed to copy soundeffect for soundeffect \"{}\"", name));
            }
        }
    }

    let sfx: SoundboardSFX = SoundboardSFX{name, icon, color, lowlatency: low};

    sfxs.push(sfx);
    return files::save_soundboard(sfxs);
}

pub(crate) fn edit_channel(color: [u8; 3], icon: String, name: String, deviceapps: String, device: bool, oldname: String, low: bool) -> Result<(), String> {
    let mut channels: Vec<Channel> = files::get_channels();

    if let Some(pos) = channels.iter().position(|c: &Channel| c.name == oldname) {
        channels[pos] = Channel{name, icon, color, device: deviceapps, deviceorapp: device, lowlatency: low, volume: 1.0};
    } else {
        eprintln!("Channel \"{}\" not found", oldname);
        return Err(format!("Channel \"{}\" not found", oldname));
    }
    
    return files::save_channels(channels).map(|_| audio::restart());
}

pub(crate) fn edit_soundboard(color: [u8; 3], icon: String, name: String, oldname: String, low: bool) -> Result<(), String> {
    let mut sfxs: Vec<SoundboardSFX> = files::get_soundboard();

    if let Some(pos) = sfxs.iter().position(|c: &SoundboardSFX| c.name == oldname) {
        sfxs[pos] = SoundboardSFX{name, icon, color, lowlatency: low};
    } else {
        eprintln!("Soundeffect \"{}\" not found", oldname);
        return Err(format!("Soundeffect \"{}\" not found", oldname));
    }
    
    return files::save_soundboard(sfxs);
}

pub(crate) fn delete_channel(name: String) -> Result<(), String> {
    let mut channels: Vec<Channel> = files::get_channels();

    if let Some(pos) = channels.iter().position(|c| c.name == name) {
        channels.remove(pos);
    } else {
        eprintln!("Channel \"{}\" not found", name);
        return Err(format!("Channel \"{}\" not found", name));
    }
    
    return files::save_channels(channels).map(|_| audio::restart());
}

pub(crate) fn delete_sound(name: String) -> Result<(), String> {
    let mut sfxs: Vec<SoundboardSFX> = files::get_soundboard();

    if let Some(pos) = sfxs.iter().position(|c: &SoundboardSFX| c.name == name) {
        sfxs.remove(pos);
    } else {
        eprintln!("Soundeffect \"{}\" not found", name);
        return Err(format!("Soundeffect \"{}\" not found", name));
    }

    let mut deleted = false;
    for ext in SFX_EXTENTIONS {
        let filename = format!("{}.{}", files::sfx_base().join(&name).to_str().unwrap_or(&name), ext);
        if fs::metadata(&filename).is_ok() {
            fs::remove_file(&filename)
                .map_err(|e| format!("Failed to delete file: {}", e))?;
            deleted = true;
            break;
        }
    }

    if !deleted {
        eprintln!("No file found for base name \"{}\"", name);
    }
    
    return files::save_soundboard(sfxs);
}

pub(crate) fn pick_menu_sound() -> Option<String> {
    if let Some(path) = rfd::FileDialog::new()
        .add_filter("Sound Files", &SFX_EXTENTIONS)
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

pub(crate) fn save_settings(output: String, scale: f32, light: bool, monitor: bool, peaks: bool, startup: bool) -> Result<(), String> {
    let mut settings: Settings = files::get_settings();
    settings.output = output;
    settings.scale = scale;
    settings.light = light;
    settings.monitor = monitor;
    settings.peaks = peaks;
    settings.startup = startup;

    files::save_settings(settings).map(|_| {audio::restart(); performance::change_bool(monitor); files::manage_startup()})
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
        eprintln!("Channel \"{}\" not found", name);
        return;
    }

    files::save_channels(channels).unwrap_or_else(|e| eprintln!("Error saving channels: {}", e));
}

pub(crate) fn get_outputs() -> Vec<String> {
    audio::outputs()
}

pub(crate) fn play_sound(name: String, low: bool) {
    let mut path: String = "".to_owned();
    for ext in SFX_EXTENTIONS {
        let filename = format!("{}.{}", files::sfx_base().join(&name).to_str().unwrap_or(&name), ext);
        if fs::metadata(&filename).is_ok() {
            path = filename;
            break;
        }
    }

    if path == "" {
        eprintln!("Failed to get soundeffect file for soundeffect \"{}\"", name);
        return;
    }

    audio::play_sfx(&path, low);
}

pub(crate) fn get_volume(name: String, get: bool, device: bool) -> String {
    audio::get_volume_parsed(name, get, device)
}

pub(crate) fn uninstall() -> Result<String, String> {
    let res: MessageDialogResult = MessageDialog::new()
        .set_title("Uninstall")
        .set_description("Are you sure you want to uninstall Vice?")
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

    let url = "https://api.github.com/repos/GlowyDeveloper/Vice/releases/latest";

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

pub(crate) fn save_blocks(item: String, blocks: String) {
    if files::blocks_base().join(format!("{}.json", item)).exists() {
        if let Err(e) = fs::remove_file(files::blocks_base().join(format!("{}.json", item))) {
            eprintln!("Failed to remove existing blocks file for item \"{}\": {:#?}", item, e);
            return;
        }
    }

    if let Err(e) = fs::write(files::blocks_base().join(format!("{}.json", item)), blocks) {
        eprintln!("Failed to write blocks file for item \"{}\": {:#?}", item, e);
    }
}

pub(crate) fn load_blocks(item: String) -> String {
    let path = files::blocks_base().join(format!("{}.json", item));
    if path.exists() {
        match fs::read_to_string(path) {
            Ok(content) => content,
            Err(e) => {
                eprintln!("Failed to read blocks file for item \"{}\": {:#?}", item, e);
                "[]".to_string()
            }
        }
    } else {
        "[]".to_string()
    }
}