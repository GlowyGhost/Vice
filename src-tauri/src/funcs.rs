use crate::files::{self, Channel, SoundboardSFX};
use crate::win::{self};

#[tauri::command]
pub(crate) fn get_soundboard() -> Option<Vec<SoundboardSFX>> {
    files::get_soundboard()
}

#[tauri::command]
pub(crate) fn get_channels() -> Option<Vec<Channel>> {
    files::get_channels()
}

#[tauri::command]
pub(crate) fn new_channel(color: [u8; 3], icon: String, name: String, device: String) -> Result<(), String> {
    match files::get_settings().unwrap() {
        None => {return Ok(());}
        Some(set) => {
            let channel = Channel{name, icon, color, device};
            let mut channels = set.channels;

            channels.push(channel);
            return files::save_channels(channels);
        }
    }
}

#[tauri::command]
pub(crate) fn new_sound(color: [u8; 3], icon: String, name: String, sound: String) -> Result<(), String> {
    match files::get_settings().unwrap() {
        None => {return Ok(());}
        Some(set) => {
            let sfx = SoundboardSFX{name, icon, color, sound};
            let mut sfxs = set.soundboard;

            sfxs.push(sfx);
            return files::save_soundboard(sfxs);
        }
    }
}

#[tauri::command]
pub(crate) fn pick_menu_sound() -> Option<String> {
    if let Some(path) = rfd::FileDialog::new()
        .add_filter("Sound Files", &["mp3", "wav", "ogg", "flac"])
        .pick_file()
    {
        Some(path.to_string_lossy().to_string())
    } else {
        None
    }
}

#[tauri::command]
pub(crate) fn get_devices() -> Vec<&'static str> {
    #[cfg(target_os = "windows")]
    return win::get_devices();
    #[cfg(target_os = "linux")]
    return lin::get_devices();
}

#[tauri::command]
pub(crate) fn flutter_print(text: &str) {
    println!("{}", text)
}

#[tauri::command]
pub(crate) fn play_sound(sound: String) {
    println!("Playing sound: {}", sound);
}