use crate::files::{self, Channel, SoundboardSFX};
use crate::audio::{self};

#[tauri::command]
pub(crate) fn get_soundboard() -> Option<Vec<SoundboardSFX>> {
    files::get_soundboard()
}

#[tauri::command]
pub(crate) fn get_channels() -> Option<Vec<Channel>> {
    files::get_channels()
}

#[tauri::command]
pub(crate) fn new_channel(color: [u8; 3], icon: String, name: String, deviceapps: String, device: bool) -> Result<(), String> {
    match files::get_settings().unwrap() {
        None => {return Ok(());}
        Some(set) => {
            let channel = Channel{name, icon, color, device: deviceapps, deviceorapp: device};
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
pub(crate) fn edit_channel(color: [u8; 3], icon: String, name: String, deviceapps: String, device: bool, oldname: String) -> Result<(), String> {
    match files::get_settings().unwrap() {
        None => {return Ok(());}
        Some(set) => {
            let mut channels = set.channels;

            if let Some(pos) = channels.iter().position(|c| c.name == oldname) {
                channels[pos] = Channel{name, icon, color, device: deviceapps, deviceorapp: device};
            } else {
                return Err(format!("Channel '{}' not found", oldname));
            }
            
            return files::save_channels(channels);
        }
    }
}

#[tauri::command]
pub(crate) fn edit_soundboard(color: [u8; 3], icon: String, name: String, sound: String, oldname: String) -> Result<(), String> {
    match files::get_settings().unwrap() {
        None => {return Ok(());}
        Some(set) => {
            let mut sfxs = set.soundboard;

            if let Some(pos) = sfxs.iter().position(|c| c.name == oldname) {
                sfxs[pos] = SoundboardSFX{name, icon, color, sound};
            } else {
                return Err(format!("Soundeffect '{}' not found", oldname));
            }
            
            return files::save_soundboard(sfxs);
        }
    }
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
    audio::get_apps()
}

#[tauri::command]
pub(crate) fn set_output(output: String) -> Result<(), String> {
    match files::get_settings().unwrap() {
        None => {return Ok(());}
        Some(mut set) => {
            set.output = output.clone();

            let res = files::save_settings(&set).map(|_| audio::restart_engine());

            res
        }
    }
}

#[tauri::command]
pub(crate) fn get_output() -> Option<String> {
    files::get_output()
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
pub(crate) fn play_sound(sound: String) {
    let _ = audio::play_sfx(&sound);
}