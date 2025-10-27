use crate::files::{self, Channel, Settings, SoundboardSFX};
use crate::audio::{self};

#[tauri::command]
pub(crate) fn get_soundboard() -> Vec<SoundboardSFX> {
    files::get_soundboard()
}

#[tauri::command]
pub(crate) fn get_channels() -> Vec<Channel> {
    files::get_channels()
}

#[tauri::command]
pub(crate) fn new_channel(color: [u8; 3], icon: String, name: String, deviceapps: String, device: bool, low: bool) -> Result<(), String> {
    let channel: Channel = Channel{name, icon, color, device: deviceapps, deviceorapp: device, lowlatency: low, volume: 1.0};
    let mut channels: Vec<Channel> = files::get_channels();

    channels.push(channel);
    return files::save_channels(channels).map(|_| audio::restart());
}

#[tauri::command]
pub(crate) fn new_sound(color: [u8; 3], icon: String, name: String, sound: String, low: bool) -> Result<(), String> {
    let sfx: SoundboardSFX = SoundboardSFX{name, icon, color, sound, lowlatency: low};
    let mut sfxs: Vec<SoundboardSFX> = files::get_soundboard();

    sfxs.push(sfx);
    return files::save_soundboard(sfxs);
}

#[tauri::command]
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

#[tauri::command]
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

#[tauri::command]
pub(crate) fn delete_channel(name: String) -> Result<(), String> {
    let mut channels: Vec<Channel> = files::get_channels();

    if let Some(pos) = channels.iter().position(|c| c.name == name) {
        channels.remove(pos);
    } else {
        return Err(format!("Channel '{}' not found", name));
    }
            
    return files::save_channels(channels).map(|_| audio::restart());
}

#[tauri::command]
pub(crate) fn delete_sound(name: String) -> Result<(), String> {
    let mut sfxs: Vec<SoundboardSFX> = files::get_soundboard();

    if let Some(pos) = sfxs.iter().position(|c: &SoundboardSFX| c.name == name) {
        sfxs.remove(pos);
    } else {
        return Err(format!("Soundeffect '{}' not found", name));
    }
            
    return files::save_soundboard(sfxs);
}

#[tauri::command]
pub(crate) fn pick_menu_sound() -> Option<String> {
    if let Some(path) = rfd::FileDialog::new()
        .add_filter("Sound Files", &["wav"])
        .pick_file()
    {
        Some(path.to_string_lossy().to_string())
    } else {
        None
    }
}

#[tauri::command]
pub(crate) fn get_devices() -> Vec<String> {
    audio::inputs()
}

#[tauri::command]
pub(crate) fn get_apps() -> Vec<String> {
    audio::apps()
}

#[tauri::command]
pub(crate) fn save_settings(output: String, scale: f32) -> Result<(), String> {
    let mut settings: Settings = files::get_settings();
    settings.output = output;
    settings.scale = scale;
    
    files::save_settings(settings).map(|_| audio::restart())
}

#[tauri::command]
pub(crate) fn get_settings() -> Settings {
    files::get_settings()
}

#[tauri::command]
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

#[tauri::command]
pub(crate) fn get_outputs() -> Vec<String> {
    audio::outputs()
}

#[tauri::command]
pub(crate) fn flutter_print(text: &str) {
    println!("{}", text)
}

#[tauri::command]
pub(crate) fn play_sound(sound: String, low: bool) {
    audio::play_sfx(&sound, low);
}